/*
 * controlsocket.{cc,hh} -- element listens to TCP/IP or Unix-domain sockets
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "controlsocket.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/llrpc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>
CLICK_DECLS

const char * const ControlSocket::protocol_version = "1.1";

struct ControlSocketErrorHandler : public BaseErrorHandler { public:

  ControlSocketErrorHandler()		{ _error_code = ControlSocket::CSERR_OK; }

  const Vector<String> &messages() const { return _messages; }
  int error_code() const		{ return _error_code; }
  
  void handle_text(Seriousness, const String &);

  void set_error_code(int c)		{ _error_code = c; }
  
 private:

  Vector<String> _messages;
  int _error_code;

};

void
ControlSocketErrorHandler::handle_text(Seriousness, const String &m)
{
  int pos = 0, nl;
  while ((nl = m.find_left('\n', pos)) >= 0) {
    _messages.push_back(m.substring(pos, nl - pos));
    pos = nl + 1;
  }
  if (pos < m.length())
    _messages.push_back(m.substring(pos));
}


ControlSocket::ControlSocket()
  : _socket_fd(-1), _proxy(0), _full_proxy(0), _retry_timer(0)
{
  MOD_INC_USE_COUNT;
}

ControlSocket::~ControlSocket()
{
  MOD_DEC_USE_COUNT;
}

int
ControlSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket (`TCP' or `UNIX')", &socktype,
		  cpIgnoreRest, cpEnd) < 0)
    return -1;

  // remove keyword arguments
  bool read_only = false, verbose = false, retry_warnings = true;
  _retries = 0;
  if (cp_va_parse_remove_keywords(conf, 2, this, errh,
		"READONLY", cpBool, "read-only?", &read_only,
		"PROXY", cpElement, "handler proxy", &_proxy,
		"VERBOSE", cpBool, "be verbose?", &verbose,
		"RETRIES", cpInteger, "number of retries", &_retries,
		"RETRY_WARNINGS", cpBool, "warn on unsuccessful socket attempt?", &retry_warnings,
		0) < 0)
    return -1;
  _read_only = read_only;
  _verbose = verbose;
  _retry_warnings = retry_warnings;
  
  socktype = socktype.upper();
  if (socktype == "TCP") {
    _tcp_socket = true;
    unsigned short portno;
    if (cp_va_parse(conf, this, errh,
		    cpIgnore, cpUnsignedShort, "port number", &portno, 0) < 0)
      return -1;
    _unix_pathname = String(portno);

  } else if (socktype == "UNIX") {
    _tcp_socket = false;
    if (cp_va_parse(conf, this, errh,
		    cpIgnore, cpString, "filename", &_unix_pathname, 0) < 0)
      return -1;
    if (_unix_pathname.length() >= (int)sizeof(((struct sockaddr_un *)0)->sun_path))
      return errh->error("filename too long");

  } else
    return errh->error("unknown socket type `%s'", socktype.cc());
  
  return 0;
}


int
ControlSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_socket_fd >= 0) {
    close(_socket_fd);
    _socket_fd = -1;
  }

  if (_retries >= 0) {
    if (_retry_warnings)
      errh->warning("%s: %s (%d %s left)", syscall, strerror(e), _retries + 1, (_retries == 0 ? "try" : "tries"));
    return -EINVAL;
  } else
    return errh->error("%s: %s", syscall, strerror(e));
}

int
ControlSocket::initialize_socket(ErrorHandler *errh)
{
  _retries--;

  // open socket, set options, bind to address
  if (_tcp_socket) {
    _socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_socket_fd < 0)
      return initialize_socket_error(errh, "socket");
    int sockopt = 1;
    if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&sockopt, sizeof(sockopt)) < 0)
      errh->warning("setsockopt: %s", strerror(errno));

    // bind to port
    int portno;
    (void) cp_integer(_unix_pathname, &portno);
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portno);
    sa.sin_addr = inet_makeaddr(0, 0);
    if (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
      return initialize_socket_error(errh, "bind");

  } else {
    _socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (_socket_fd < 0)
      return initialize_socket_error(errh, "socket");

    // bind to port
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path, _unix_pathname.cc(), _unix_pathname.length() + 1);
    if (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
      return initialize_socket_error(errh, "bind");
  }

  // start listening
  if (listen(_socket_fd, 2) < 0)
    return initialize_socket_error(errh, "listen");
  
  // nonblocking I/O and close-on-exec for the socket
  fcntl(_socket_fd, F_SETFL, O_NONBLOCK);
  fcntl(_socket_fd, F_SETFD, FD_CLOEXEC);

  add_select(_socket_fd, SELECT_READ);
  return 0;
}

void
ControlSocket::retry_hook(Timer *t, void *thunk)
{
  ControlSocket *cs = (ControlSocket *)thunk;
  if (cs->_socket_fd >= 0)
    /* nada */;
  else if (cs->initialize_socket(ErrorHandler::default_handler()) >= 0)
    /* nada */;
  else if (cs->_retries >= 0)
    t->reschedule_after_s(1);
  else
    cs->router()->please_stop_driver();
}

int
ControlSocket::initialize(ErrorHandler *errh)
{
  // check for a full proxy
  if (_proxy)
    if (void *v = _proxy->cast("HandlerProxy"))
      _full_proxy = (HandlerProxy *)v;
  
  // ask the proxy to send us errors
  if (_full_proxy)
    _full_proxy->add_error_receiver(proxy_error_function, this);

  if (initialize_socket(errh) >= 0)
    return 0;
  else if (_retries >= 0) {
    _retry_timer = new Timer(retry_hook, this);
    _retry_timer->initialize(this);
    _retry_timer->schedule_after_s(1);
    return 0;
  } else
    return -1;
}

void
ControlSocket::take_state(Element *e, ErrorHandler *errh)
{
  ControlSocket *cs = (ControlSocket *)e->cast("ControlSocket");
  if (!cs)
    return;

  if (_socket_fd >= 0) {
    errh->error("already initialized, can't take state");
    return;
  } else if (_tcp_socket != cs->_tcp_socket
	     || _unix_pathname != cs->_unix_pathname) {
    errh->error("incompatible ControlSockets");
    return;
  }

  _socket_fd = cs->_socket_fd;
  cs->_socket_fd = -1;
  _in_texts.swap(cs->_in_texts);
  _out_texts.swap(cs->_out_texts);
  _flags.swap(cs->_flags);

  if (_socket_fd >= 0)
    add_select(_socket_fd, SELECT_READ);
  for (int i = 0; i < _flags.size(); i++)
    if (_flags[i] >= 0)
      add_select(i, SELECT_READ | SELECT_WRITE);
}

void
ControlSocket::cleanup(CleanupStage)
{
  if (_full_proxy)
    _full_proxy->remove_error_receiver(proxy_error_function, this);
  if (_socket_fd >= 0) {
    // shut down the listening socket in case we forked
#ifdef SHUT_RDWR
    shutdown(_socket_fd, SHUT_RDWR);
#else
    shutdown(_socket_fd, 2);
#endif
    close(_socket_fd);
    if (!_tcp_socket)
      unlink(_unix_pathname.c_str());
    _socket_fd = -1;
  }
  for (int i = 0; i < _flags.size(); i++)
    if (_flags[i] >= 0) {
      close(i);
      _flags[i] = -1;
    }
  if (_retry_timer) {
    _retry_timer->cleanup();
    delete _retry_timer;
    _retry_timer = 0;
  }
}

int
ControlSocket::message(int fd, int code, const String &s, bool continuation)
{
  assert(code >= 100 && code <= 999);
  if (fd >= 0 && !(_flags[fd] & WRITE_CLOSED))
    _out_texts[fd] += String(code) + (continuation ? "-" : " ") + s.printable() + "\r\n";
  return ANY_ERR;
}

int
ControlSocket::transfer_messages(int fd, int default_code, const String &msg,
				 ControlSocketErrorHandler *errh)
{
  int code = errh->error_code();
  if (code == CSERR_OK)
    code = default_code;
  const Vector<String> &messages = errh->messages();
  
  if (msg) {
    if (messages.size() > 0)
      message(fd, code, msg + ":", true);
    else
      message(fd, code, msg, false);
  }
  
  for (int i = 0; i < messages.size(); i++)
    message(fd, code, messages[i], i < messages.size() - 1);
  
  return ANY_ERR;
}

static String
canonical_handler_name(const String &n)
{
  int dot = n.find_left('.');
  if (dot == 0 || (dot == 1 && n[0] == '0'))
    return n.substring(dot + 1);
  else
    return n;
}

String
ControlSocket::proxied_handler_name(const String &n) const
{
  if (_full_proxy && n.find_left('.') < 0)
    return "0." + n;
  else
    return n;
}

int
ControlSocket::parse_handler(int fd, const String &full_name, Element **es)
{
  // Parse full_name into element_name and handler_name.
  String canonical_name = canonical_handler_name(full_name);

  // Check for proxy.
  if (_proxy) {
    // collect errors from proxy
    ControlSocketErrorHandler errh;
    _proxied_handler = proxied_handler_name(canonical_name);
    _proxied_errh = &errh;
    
    int hid = Router::hindex(_proxy, _proxied_handler);
    
    if (errh.nerrors() > 0)
      return transfer_messages(fd, CSERR_NO_SUCH_HANDLER, String(), &errh);
    else if (hid < 0)
      return message(fd, CSERR_NO_SUCH_HANDLER, "No proxied handler named `" + full_name + "'");
    
    *es = _proxy;
    return hid;
  }

  // Otherwise, find element.
  Element *e;
  int dot = canonical_name.find_left('.');
  String hname;
  
  if (dot >= 0) {
    String ename = canonical_name.substring(0, dot);
    e = router()->find(ename);
    if (!e) {
      int num;
      if (cp_integer(ename, &num) && num > 0 && num <= router()->nelements())
	e = router()->element(num - 1);
    }
    if (!e)
      return message(fd, CSERR_NO_SUCH_ELEMENT, "No element named `" + ename + "'");
    hname = canonical_name.substring(dot + 1);
  } else {
    e = router()->root_element();
    hname = canonical_name;
  }

  // Then find handler.
  int hid = Router::hindex(e, hname);
  if (hid < 0 || !router()->handler(hid)->visible())
    return message(fd, CSERR_NO_SUCH_HANDLER, "No handler named `" + full_name + "'");

  // Return.
  *es = e;
  return hid;
}

int
ControlSocket::read_command(int fd, const String &handlername)
{
  Element *e;
  int hid = parse_handler(fd, handlername, &e);
  if (hid < 0)
    return hid;
  const Router::Handler *h = router()->handler(hid);
  if (!h->read_visible())
    return message(fd, CSERR_PERMISSION, "Handler `" + handlername + "' write-only");

  // collect errors from proxy
  ControlSocketErrorHandler errh;
  _proxied_handler = h->name();
  _proxied_errh = &errh;
  
  String data = h->call_read(e);

  // did we get an error message?
  if (errh.nerrors() > 0)
    return transfer_messages(fd, CSERR_UNSPECIFIED, String(), &errh);
  
  message(fd, CSERR_OK, "Read handler `" + handlername + "' OK");
  _out_texts[fd] += "DATA " + String(data.length()) + "\r\n";
  _out_texts[fd] += data;
  return 0;
}

int
ControlSocket::write_command(int fd, const String &handlername, const String &data)
{
  Element *e;
  int hid = parse_handler(fd, handlername, &e);
  if (hid < 0)
    return hid;
  const Router::Handler *h = router()->handler(hid);
  if (!h->writable())
    return message(fd, CSERR_PERMISSION, "Handler `" + handlername + "' read-only");

  if (_read_only)
    return message(fd, CSERR_PERMISSION, "Permission denied for `" + handlername + "'");

#ifdef LARGEST_HANDLER_WRITE
  if (data.length() > LARGEST_HANDLER_WRITE)
    return message(fd, CSERR_DATA_TOO_BIG, "Data too large for write handler `" + handlername + "'");
#endif
  
  ControlSocketErrorHandler errh;
  
  // call handler
  int result = h->call_write(data, e, &errh);

  // add a generic error message for certain handler codes
  int code = errh.error_code();
  if (code == CSERR_OK) {
    if (errh.nerrors() > 0 || result < 0)
      code = CSERR_HANDLER_ERROR;
    else if (errh.nwarnings() > 0)
      code = CSERR_OK_HANDLER_WARNING;
  }

  String msg;
  if (code == CSERR_OK)
    msg = "Write handler `" + handlername + "' OK";
  else if (code == CSERR_OK_HANDLER_WARNING)
    msg = "Write handler `" + handlername + "' OK with warnings";
  else if (code == CSERR_HANDLER_ERROR)
    msg = "Write handler `" + handlername + "' error";
  transfer_messages(fd, code, msg, &errh);
  return 0;
}

int
ControlSocket::check_command(int fd, const String &hname, bool write)
{
  int ok = 0;
  int any_visible = 0;
  ControlSocketErrorHandler errh;

  if (_full_proxy) {
    String phname = canonical_handler_name(hname);
    if (phname.find_left('.') < 0)
      phname = "0." + phname;
    ok = _full_proxy->check_handler(phname, write, &errh);
  } else {
    Element *e;
    int hid = parse_handler(fd, hname, &e);
    if (hid < 0)
      return 0;			// error messages already reported
    const Router::Handler *h = router()->handler(hid);
    ok = (h->visible() && (write ? h->write_visible() : h->read_visible()));
    any_visible = h->visible();
  }

  // remember _read_only!
  if (write && _read_only && ok)
    return message(fd, CSERR_PERMISSION, "Permission denied for `" + hname + "'");
  else if (errh.messages().size() > 0)
    transfer_messages(fd, CSERR_OK, String(), &errh);
  else if (ok)
    message(fd, CSERR_OK, String(write ? "Write" : "Read") + " handler `" + hname + "' OK");
  else if (any_visible)
    message(fd, CSERR_NO_SUCH_HANDLER, "Handler `" + hname + (write ? "' not writable" : "' not readable"));
  else
    message(fd, CSERR_NO_SUCH_HANDLER, "No " + String(write ? "write" : "read") + " handler named `" + hname + "'");
  return 0;
}

int
ControlSocket::parse_command(int fd, const String &line)
{
  // split `line' into words; don't use cp_ functions since they strip comments
  Vector<String> words;
  const char *data = line.data();
  int len = line.length();
  for (int pos = 0; pos < len; ) {
    while (pos < len && isspace(data[pos]))
      pos++;
    int first = pos;
    while (pos < len && !isspace(data[pos]))
      pos++;
    if (first < pos)
      words.push_back(line.substring(first, pos - first));
  }
  if (words.size() == 0)
    return 0;

  // branch on command
  String command = words[0].upper();
  if (command == "READ" || command == "GET") {
    if (words.size() != 2)
      return message(fd, CSERR_SYNTAX, "Wrong number of arguments");
    return read_command(fd, words[1]);
    
  } else if (command == "WRITE" || command == "SET") {
    if (words.size() < 2)
      return message(fd, CSERR_SYNTAX, "Wrong number of arguments");
    String data;
    for (int i = 2; i < words.size(); i++)
      data += (i == 2 ? "" : " ") + words[i];
    return write_command(fd, words[1], data);
    
  } else if (command == "WRITEDATA" || command == "SETDATA") {
    if (words.size() != 3)
      return message(fd, CSERR_SYNTAX, "Wrong number of arguments");
    int datalen;
    if (!cp_integer(words[2], &datalen) || datalen < 0)
      return message(fd, CSERR_SYNTAX, "Syntax error in `writedata'");
    if (_in_texts[fd].length() < datalen) {
      if (_flags[fd] & READ_CLOSED)
	return message(fd, CSERR_SYNTAX, "Not enough data");
      else			// retry
	return 1;
    }
    String data = _in_texts[fd].substring(0, datalen);
    _in_texts[fd] = _in_texts[fd].substring(datalen);
    return write_command(fd, words[1], data);

  } else if (command == "CHECKREAD") {
    if (words.size() != 2)
      return message(fd, CSERR_SYNTAX, "Wrong number of arguments");
    return check_command(fd, words[1], false);
    
  } else if (command == "CHECKWRITE") {
    if (words.size() != 2)
      return message(fd, CSERR_SYNTAX, "Wrong number of arguments");
    return check_command(fd, words[1], true);
    
  } else if (command == "CLOSE" || command == "QUIT") {
    if (words.size() != 1)
      message(fd, CSERR_SYNTAX, "Bad command syntax");
    message(fd, CSERR_OK, "Goodbye!");
    _flags[fd] |= READ_CLOSED;
    _in_texts[fd] = String();
    return 0;
    
  } else
    return message(fd, CSERR_UNIMPLEMENTED, "Command `" + command + "' unimplemented");
}

void
ControlSocket::flush_write(int fd)
{
  assert(_flags[fd] >= 0);
  if (!(_flags[fd] & WRITE_CLOSED)) {
    int w = 0;
    while (_out_texts[fd].length()) {
      const char *x = _out_texts[fd].data();
      w = write(fd, x, _out_texts[fd].length());
      if (w < 0 && errno != EINTR)
	break;
      if (w > 0)
	_out_texts[fd] = _out_texts[fd].substring(w);
    }
    if (w < 0 && errno == EPIPE)
      _flags[fd] |= WRITE_CLOSED;
    // don't select writes unless we have data to write
    if (_out_texts[fd].length())
      add_select(fd, SELECT_WRITE);
    else
      remove_select(fd, SELECT_WRITE);
  }
}

void
ControlSocket::selected(int fd)
{
  if (fd == _socket_fd) {
    union { struct sockaddr_in in; struct sockaddr_un un; } sa;
#ifdef __APPLE__
    int sa_len;
#else
    socklen_t sa_len;
#endif
    sa_len = sizeof(sa);
    int new_fd = accept(_socket_fd, (struct sockaddr *)&sa, &sa_len);

    if (new_fd < 0) {
      if (errno != EAGAIN)
	click_chatter("%s: accept: %s", declaration().cc(), strerror(errno));
      return;
    }

    if (_verbose) {
      if (_tcp_socket)
	click_chatter("%s: opened connection %d from %s.%d", declaration().cc(), new_fd, IPAddress(sa.in.sin_addr).unparse().cc(), ntohs(sa.in.sin_port));
      else
	click_chatter("%s: opened connection %d", declaration().cc(), new_fd);
    }

    fcntl(new_fd, F_SETFL, O_NONBLOCK);
    fcntl(new_fd, F_SETFD, FD_CLOEXEC);
    add_select(new_fd, SELECT_READ | SELECT_WRITE);

    while (new_fd >= _in_texts.size()) {
      _in_texts.push_back(String());
      _out_texts.push_back(String());
      _flags.push_back(-1);
    }
    _in_texts[new_fd] = String();
    _out_texts[new_fd] = String();
    _flags[new_fd] = 0;

    fd = new_fd;
    _out_texts[new_fd] = "Click::ControlSocket/" + String(protocol_version) + "\r\n";
  }

  // find file descriptor
  if (fd >= _in_texts.size() || _flags[fd] < 0)
    return;

  // read commands from socket
  if (!(_flags[fd] & READ_CLOSED)) {
    char buf[2048];
    int r;
    while (1) {
      while ((r = read(fd, buf, 2048)) > 0)
	_in_texts[fd].append(buf, r);
      if ((r < 0 && errno != EINTR) || r == 0)
	break;
    }
    if (r == 0 || (r < 0 && errno != EAGAIN))
      _flags[fd] |= READ_CLOSED;
  }
  
  // parse commands
  while (_in_texts[fd].length()) {
    const char *in_text = _in_texts[fd].data();
    int len = _in_texts[fd].length();
    int pos = 0;
    while (pos < len && in_text[pos] != '\r' && in_text[pos] != '\n')
      pos++;
    if (pos >= len && !(_flags[fd] & READ_CLOSED)) // incomplete command
      break;

    // include end of line
    if (pos < len - 1 && in_text[pos] == '\r' && in_text[pos+1] == '\n')
      pos += 2;
    else if (pos < len)		// '\r' or '\n' alone
      pos++;
    
    // grab string
    String old_text = _in_texts[fd];
    String line = old_text.substring(0, pos);
    _in_texts[fd] = old_text.substring(pos);

    // parse each individual command
    if (parse_command(fd, line) > 0) {
      // more data to come, so wait
      _in_texts[fd] = old_text;
      break;
    }
  }

  // write data until blocked
  flush_write(fd);

  // maybe close out
  if (((_flags[fd] & READ_CLOSED) && !_out_texts[fd].length())
      || (_flags[fd] & WRITE_CLOSED)) {
    close(fd);
    remove_select(fd, SELECT_READ | SELECT_WRITE);
    if (_verbose)
      click_chatter("%s: closed connection %d", declaration().cc(), fd);
    _flags[fd] = -1;
  }
}


ErrorHandler *
ControlSocket::proxy_error_function(const String &h, void *thunk)
{
  ControlSocket *cs = static_cast<ControlSocket *>(thunk);
  return (h == cs->_proxied_handler ? cs->_proxied_errh : 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ControlSocket)
