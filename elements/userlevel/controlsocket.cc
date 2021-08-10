/*
 * controlsocket.{cc,hh} -- element listens to TCP/IP or Unix-domain sockets
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2008 Meraki, Inc.
 * Copyright (c) 2004-2011 Regents of the University of California
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
#include <click/args.hh>
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

const char ControlSocket::protocol_version[] = "1.3";

class ControlSocketErrorHandler : public ErrorHandler { public:

    ControlSocketErrorHandler()
	: _error_code(ControlSocket::CSERR_OK), _nwarnings(0) {
    }

    const Vector<String> &messages() const {
	return _messages;
    }
    int error_code() const {
	return _error_code;
    }
    int nwarnings() const {
	return _nwarnings;
    }

    void *emit(const String &str, void *user_data, bool more);
    void account(int level);

  private:

    Vector<String> _messages;
    int _error_code;
    int _nwarnings;

};

void *
ControlSocketErrorHandler::emit(const String &str, void *, bool)
{
    String landmark;
    const char *s = parse_anno(str, str.begin(), str.end(), "l", &landmark,
			       "#cserr", &_error_code, (const char *) 0);
    _messages.push_back(clean_landmark(landmark, true)
			+ str.substring(s, str.end()));
    return 0;
}

void
ControlSocketErrorHandler::account(int level)
{
    ErrorHandler::account(level);
    if (level == el_warning)
	++_nwarnings;
}


ControlSocket::ControlSocket()
  : _socket_fd(-1), _proxy(0), _full_proxy(0), _retry_timer(0)
{
}

ControlSocket::~ControlSocket()
{
}

int
ControlSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String socktype;
    Args args = Args(this, errh).bind(conf);
    if (args.read_mp("TYPE", socktype).execute() < 0)
	return -1;

    // remove keyword arguments
    bool read_only = false, verbose = false, retry_warnings = true, localhost = false;
    _retries = 0;
    if (args.read("READONLY", read_only)
	.read("PROXY", _proxy)
	.read("VERBOSE", verbose)
	.read("RETRIES", _retries)
	.read("RETRY_WARNINGS", retry_warnings)
	.read("LOCALHOST", localhost)
	.consume() < 0)
	return -1;
    _read_only = read_only;
    _verbose = verbose;
    _retry_warnings = retry_warnings;
    _localhost = localhost;

    socktype = socktype.upper();
    if (socktype == "TCP") {
	_type = type_tcp;
	if (args.read_mp("PORT", WordArg(), _unix_pathname)
	    .complete() < 0)
	    return -1;
	uint16_t portno;
	int portno_int = 0;
	if (IPPortArg(IP_PROTO_TCP).parse(_unix_pathname, portno, this))
	    _unix_pathname = String(portno);
	else if (_unix_pathname && _unix_pathname.back() == '+'
		 && IntArg().parse(_unix_pathname.substring(0, -1), portno_int)
		 && portno_int > 0 && portno_int < 65536)
	    _unix_pathname = String(portno_int) + "+";
	else
	    return errh->error("PORT requires TCP port");

    } else if (socktype == "UNIX") {
	_type = type_unix;
	if (args.read_mp("FILENAME", FilenameArg(), _unix_pathname)
	    .complete() < 0)
	    return -1;
	if (_unix_pathname.length() >= (int)sizeof(((struct sockaddr_un *)0)->sun_path))
	    return errh->error("filename too long");

    } else if (socktype == "SOCKET") {
	_type = type_socket;
	int fd;
	if (args.read_mp("FD", fd).complete() < 0)
	    return -1;
	_unix_pathname = String(fd);

    } else
	return errh->error("unknown socket type %<%s%>", socktype.c_str());

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
  if (_type == type_tcp) {
    _socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_socket_fd < 0)
      return initialize_socket_error(errh, "socket");
    int sockopt = 1;
    if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&sockopt, sizeof(sockopt)) < 0)
      errh->warning("setsockopt: %s", strerror(errno));

    // bind to port
    int portno;
    (void) cp_integer(_unix_pathname.begin(), _unix_pathname.end(), 0, &portno);
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portno);
    if (_localhost)
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
    int tries = 0;
    while (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
	if (tries > 10 || _unix_pathname.back() != '+' || portno >= 65534
	    || (_retries >= 0 && hotswap_element()))
	    return initialize_socket_error(errh, "bind");
	++portno, ++tries;
	sa.sin_port = htons(portno);
    }
    _unix_pathname = String(portno);

  } else if (_type == type_unix) {
    _socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (_socket_fd < 0)
      return initialize_socket_error(errh, "socket");

    // bind to port
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path, _unix_pathname.c_str(), _unix_pathname.length() + 1);
    unlink(_unix_pathname.c_str());
    if (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
      return initialize_socket_error(errh, "bind");

  } else if (_type == type_socket) {
      if (!hotswap_element()) {
	  int fd;
	  (void) Args().push_back_words(_unix_pathname).read_mp("FD", fd).execute();
	  initialize_connection(fd);
      }
      return 0;
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
    t->reschedule_after_sec(1);
  else
    cs->router()->please_stop_driver();
}

int
ControlSocket::initialize(ErrorHandler *errh)
{
  // check for a full proxy
  if (_proxy)
    _full_proxy = static_cast<HandlerProxy *>(_proxy->cast("HandlerProxy"));

  // ask the proxy to send us errors
  if (_full_proxy)
    _full_proxy->add_error_receiver(proxy_error_function, this);

  if (initialize_socket(errh) >= 0)
    return 0;
  else if (_retries >= 0) {
    _retry_timer = new Timer(retry_hook, this);
    _retry_timer->initialize(this);
    _retry_timer->schedule_after_sec(1);
    return 0;
  } else
    return -1;
}

Element *
ControlSocket::hotswap_element() const
{
    if (Element *e = Element::hotswap_element())
	if (ControlSocket *cs = (ControlSocket *)e->cast("ControlSocket"))
	    if (cs->_type == _type
		&& (cs->_unix_pathname == _unix_pathname
		    || (_type == type_tcp && _unix_pathname.back() == '+')))
		return cs;
    return 0;
}

void
ControlSocket::take_state(Element *e, ErrorHandler *errh)
{
    ControlSocket *cs = (ControlSocket *) e->cast("ControlSocket");

    if (_socket_fd >= 0) {
	errh->error("already initialized, can't take state");
	return;
    }

    _socket_fd = cs->_socket_fd;
    _unix_pathname = cs->_unix_pathname; // in case _unix_pathname == "41930+"
    cs->_socket_fd = -1;
    _conns.swap(cs->_conns);

    if (_socket_fd >= 0)
	add_select(_socket_fd, SELECT_READ);
    for (connection **it = _conns.begin(); it != _conns.end(); ++it) {
	if (*it && !(*it)->in_closed)
	    add_select((*it)->fd, SELECT_READ);
	if (*it && !(*it)->out_closed)
	    add_select((*it)->fd, SELECT_WRITE);
    }
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
	if (_type == type_unix)
	    unlink(_unix_pathname.c_str());
	_socket_fd = -1;
    }
    for (connection **it = _conns.begin(); it != _conns.end(); ++it)
	if (*it) {
	    (*it)->flush_write(this, false);	// try one last time to emit all data
	    close((*it)->fd);
	    delete *it;
	}
    if (_retry_timer) {
	delete _retry_timer;
	_retry_timer = 0;
    }
}

int
ControlSocket::connection::message(int code, const String &msg, bool continuation)
{
    assert(code >= 100 && code <= 999);
    if (fd >= 0 && !out_closed)
	out_text << code << (continuation ? '-' : ' ') << msg.printable() << '\r' << '\n';
    return ANY_ERR;
}

int
ControlSocket::connection::transfer_messages(int default_code, const String &msg,
					     ControlSocketErrorHandler *errh)
{
    int code = errh->error_code();
    if (code == CSERR_OK)
	code = default_code;
    const Vector<String> &messages = errh->messages();

    if (msg) {
	if (messages.size() > 0)
	    message(code, msg + ":", true);
	else
	    message(code, msg, false);
    }

    for (int i = 0; i < messages.size(); i++)
	message(code, messages[i], i < messages.size() - 1);

    return ANY_ERR;
}

void
ControlSocket::connection::contract(StringAccum &sa, int &pos)
{
    if (pos == sa.length()) {
	sa.clear();
	pos = 0;
    } else if (pos > 8192) {
	memmove(sa.data(), sa.data() + pos, sa.length() - pos);
	sa.resize(sa.length() - pos);
	pos = 0;
    }
}

int
ControlSocket::connection::read_insufficient()
{
    if (in_closed)
	return message(CSERR_SYNTAX, "Not enough data");
    else			// retry
	return 1;
}

int
ControlSocket::connection::read(int len, String &data)
{
    if (in_text.length() < inpos + len)
	return read_insufficient();
    data = String(in_text.begin() + inpos, in_text.begin() + inpos + len);
    inpos += len;
    return 0;
}

void
ControlSocket::connection::flush_write(ControlSocket *cs, bool read_needs_processing)
{
    if (!out_closed) {
	ssize_t w = 0;
	while (outpos < out_text.length()) {
	    w = write(fd, out_text.data() + outpos, out_text.length() - outpos);
	    if (w == -1 && errno != EINTR)
		break;
	    if (w != 0 && w != -1)
		outpos += w;
	}
	if (w == -1 && errno == EPIPE)
	    out_closed = true;
	contract(out_text, outpos);
	// don't select writes unless we have data to write (or read needs more
	// processing)
	if (out_text.length() || read_needs_processing)
	    cs->add_select(fd, Element::SELECT_WRITE);
	else
	    cs->remove_select(fd, Element::SELECT_WRITE);
    }
}

static String
canonical_handler_name(const String &n)
{
  const char *dot = find(n, '.');
  if (dot == n.begin() || (dot == n.begin() + 1 && n.front() == '0'))
    return n.substring(dot + 1, n.end());
  else
    return n;
}

String
ControlSocket::proxied_handler_name(const String &n) const
{
  if (_full_proxy && find(n, '.') == n.end())
    return "." + n;
  else
    return n;
}

const Handler*
ControlSocket::parse_handler(connection &conn, const String &full_name, Element **es)
{
  // Parse full_name into element_name and handler_name.
  String canonical_name = canonical_handler_name(full_name);

  // Check for proxy.
  if (_proxy) {
    // collect errors from proxy
    ControlSocketErrorHandler errh;
    _proxied_handler = proxied_handler_name(canonical_name);
    _proxied_errh = &errh;
    const Handler* h = Router::handler(_proxy, _proxied_handler);
    _proxied_errh = 0;

    if (errh.nerrors() > 0) {
      conn.transfer_messages(CSERR_NO_SUCH_HANDLER, String(), &errh);
      return 0;
    } else if (!h) {
      conn.message(CSERR_NO_SUCH_HANDLER, "No proxied handler named '" + full_name + "'");
      return 0;
    } else {
      *es = _proxy;
      return h;
    }
  }

  // Otherwise, find element.
  Element *e;
  const char *dot = find(canonical_name, '.');
  String hname;

  if (dot != canonical_name.end()) {
    String ename = canonical_name.substring(canonical_name.begin(), dot);
    e = router()->find(ename);
    if (!e) {
      int num;
      if (IntArg().parse(ename, num) && num > 0 && num <= router()->nelements())
	e = router()->element(num - 1);
    }
    if (!e) {
      conn.message(CSERR_NO_SUCH_ELEMENT, "No element named '" + ename + "'");
      return 0;
    }
    hname = canonical_name.substring(dot + 1, canonical_name.end());
  } else {
    e = router()->root_element();
    hname = canonical_name;
  }

  // Then find handler.
  const Handler* h = Router::handler(e, hname);
  if (h && h->visible()) {
    *es = e;
    return h;
  } else {
    conn.message(CSERR_NO_SUCH_HANDLER, "No handler named '" + full_name + "'");
    return 0;
  }
}

int
ControlSocket::read_command(connection &conn, const String &handlername, String param)
{
  Element *e;
  const Handler* h = parse_handler(conn, handlername, &e);
  if (!h)
    return ANY_ERR;
  else if (!h->read_visible())
    return conn.message(CSERR_PERMISSION, "Handler '" + handlername + "' write-only");

  // collect errors from proxy
  ControlSocketErrorHandler errh;
  _proxied_handler = h->name();
  _proxied_errh = &errh;
  String data = h->call_read(e, param, &errh);
  _proxied_errh = 0;

  // did we get an error message?
  if (errh.nerrors() > 0)
    return conn.transfer_messages(CSERR_UNSPECIFIED, "Read handler '" + handlername + "' error", &errh);

  conn.message(CSERR_OK, "Read handler '" + handlername + "' OK");
  conn.out_text << "DATA " << data.length() << '\r' << '\n' << data;
  return 0;
}

int
ControlSocket::write_command(connection &conn, const String &handlername, String data)
{
  Element *e;
  const Handler* h = parse_handler(conn, handlername, &e);
  if (!h)
    return ANY_ERR;
  else if (!h->writable())
    return conn.message(CSERR_PERMISSION, "Handler '" + handlername + "' read-only");

  if (_read_only)
    return conn.message(CSERR_PERMISSION, "Permission denied for '" + handlername + "'");

#ifdef LARGEST_HANDLER_WRITE
  if (data.length() > LARGEST_HANDLER_WRITE)
    return conn.message(CSERR_DATA_TOO_BIG, "Data too large for write handler '" + handlername + "'");
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
    msg = "Write handler '" + handlername + "' OK";
  else if (code == CSERR_OK_HANDLER_WARNING)
    msg = "Write handler '" + handlername + "' OK with warnings";
  else if (code == CSERR_HANDLER_ERROR)
    msg = "Write handler '" + handlername + "' error";
  conn.transfer_messages(code, msg, &errh);
  return 0;
}

int
ControlSocket::check_command(connection &conn, const String &hname, bool write)
{
  int ok = 0;
  int any_visible = 0;
  ControlSocketErrorHandler errh;

  if (_full_proxy) {
    String phname = canonical_handler_name(hname);
    if (find(phname, '.') == phname.end())
      phname = "." + phname;
    ok = _full_proxy->check_handler(phname, write, &errh);
  } else {
    Element *e;
    const Handler* h = parse_handler(conn, hname, &e);
    if (!h)
      return 0;			// error messages already reported
    ok = (h->visible() && (write ? h->write_visible() : h->read_visible()));
    any_visible = h->visible();
  }

  // remember _read_only!
  if (write && _read_only && ok)
    return conn.message(CSERR_PERMISSION, "Permission denied for '" + hname + "'");
  else if (errh.messages().size() > 0)
    conn.transfer_messages(CSERR_OK, String(), &errh);
  else if (ok)
    conn.message(CSERR_OK, String(write ? "Write" : "Read") + " handler '" + hname + "' OK");
  else if (any_visible)
    conn.message(CSERR_NO_SUCH_HANDLER, "Handler '" + hname + (write ? "' not writable" : "' not readable"));
  else
    conn.message(CSERR_NO_SUCH_HANDLER, "No " + String(write ? "write" : "read") + " handler named '" + hname + "'");
  return 0;
}

int
ControlSocket::llrpc_command(connection &conn, const String &llrpcname, String data)
{
  const char *octothorp = find(llrpcname, '#');
  uint32_t command = 0;
  if (!IntArg(16).parse(llrpcname.substring(octothorp + 1, llrpcname.end()), command))
    return conn.message(CSERR_SYNTAX, "Syntax error in LLRPC name '" + llrpcname + "'");
  // transform net LLRPC id into host LLRPC id
  command = CLICK_LLRPC_NTOH(command);

  Element *e;
  const Handler* h = parse_handler(conn, llrpcname.substring(llrpcname.begin(), octothorp) + ".name", &e);
  if (!h)
    return ANY_ERR;

  int size = _CLICK_IOC_SIZE(command);
  if (!size || !(command & (_CLICK_IOC_IN | _CLICK_IOC_OUT)) || !(command & _CLICK_IOC_FLAT))
    return conn.message(CSERR_UNIMPLEMENTED, "Cannot call LLRPC '" + llrpcname + "' remotely");

  if (_read_only)		// can't tell whether an LLRPC is read-only;
				// so disallow them all
    return conn.message(CSERR_PERMISSION, "Permission denied for '" + llrpcname + "'");

  if ((command & _CLICK_IOC_IN) && data.length() != size)
    return conn.message(CSERR_LLRPC_ERROR, "LLRPC '" + llrpcname + "' requires " + String(size) + " bytes input data");
  else if (!(command & _CLICK_IOC_IN))
    data = String::make_uninitialized(size);

  // collect errors from proxy
  ControlSocketErrorHandler errh;
  _proxied_handler = llrpcname;
  _proxied_errh = &errh;

  int retval;
  if (_proxy) {
    struct click_llrpc_proxy_st pst;
    pst.proxied_handler = (void*) h;
    pst.proxied_command = command;
    pst.proxied_data = data.mutable_data();
    retval = _proxy->llrpc(CLICK_LLRPC_PROXY, &pst);
  } else
    retval = e->llrpc(command, data.mutable_data());

  _proxied_errh = 0;

  // did we get an error message?
  String msg;
  if (retval < 0)
    msg = "LLRPC '" + llrpcname + "' error: " + String(strerror(-retval));
  else if (errh.nerrors() > 0)
    msg = "LLRPC '" + llrpcname + "' error";
  else
    msg = "LLRPC '" + llrpcname + "' OK";
  int code = (retval < 0 || errh.nerrors() > 0 ? CSERR_LLRPC_ERROR : CSERR_OK);
  conn.transfer_messages(code, msg, &errh);

  if (code == CSERR_OK) {
    if (!(command & _CLICK_IOC_OUT))
      data = String();
    conn.out_text << "DATA " << data.length() << '\r' << '\n' << data;
  }
  return 0;
}

int
ControlSocket::parse_command(connection &conn, const String &line)
{
  // split 'line' into words; don't use cp_ functions since they strip comments
  Vector<String> words;
  const char *data = line.data();
  int len = line.length();
  for (int pos = 0; pos < len; ) {
      while (pos < len && isspace((unsigned char) data[pos]))
      pos++;
    int first = pos;
    while (pos < len && !isspace((unsigned char) data[pos]))
      pos++;
    if (first < pos)
      words.push_back(line.substring(first, pos - first));
  }
  if (words.size() == 0)
    return 0;

  // branch on command
  String command = words[0].upper();
  if (command == "READ" || command == "GET" || command == "WRITE"
      || command == "SET") {
      if (words.size() < 2)
	  return conn.message(CSERR_SYNTAX, "Wrong number of arguments");
      String data;
      if (words.size() > 2)
	  data = line.substring(words[2].begin(), words.back().end());
      if (command[0] == 'R' || command[0] == 'G')
	  return read_command(conn, words[1], data);
      else
	  return write_command(conn, words[1], data);

  } else if (command == "READDATA" || command == "WRITEDATA"
	     || command == "SETDATA") {
      if (words.size() != 3)
	  return conn.message(CSERR_SYNTAX, "Wrong number of arguments");
      int datalen, r;
      if (!IntArg().parse(words[2], datalen) || datalen < 0)
	  return conn.message(CSERR_SYNTAX, "Syntax error in '%s'", command.c_str());
      String data;
      if ((r = conn.read(datalen, data)) != 0)
	  return r;
      if (command[0] == 'R')
	  return read_command(conn, words[1], data);
      else
	  return write_command(conn, words[1], data);

  } else if (command == "READUNTIL" || command == "WRITEUNTIL") {
      if (words.size() < 2)
	  return conn.message(CSERR_SYNTAX, "Wrong number of arguments");
      String until;
      if (words.size() > 2)
	  until = line.substring(words[2].begin(), words.back().end());
      const char *s = conn.in_text.begin() + conn.inpos,
	  *end = conn.in_text.end(), *linebegin, *lineend;
      while (1) {
	  linebegin = lineend = s;
	  for (; s != end && *s != '\n' && *s != '\r'; ++s)
	      if (!isspace((unsigned char) *s))
		  lineend = s + 1;
	  if (s == end)
	      return conn.read_insufficient();
	  if (*s == '\r' && s + 1 != end && s[1] == '\n')
	      s += 2;
	  else
	      ++s;
	  if (lineend - linebegin == until.length()
	      && memcmp(linebegin, until.begin(), until.length()) == 0)
	      break;
      }
      String data(conn.in_text.begin() + conn.inpos, linebegin);
      conn.inpos = s - conn.in_text.begin();
      if (command[0] == 'R')
	  return read_command(conn, words[1], data);
      else
	  return write_command(conn, words[1], data);

  } else if (command == "CHECKREAD" || command == "CHECKWRITE") {
      if (words.size() != 2)
	  return conn.message(CSERR_SYNTAX, "Wrong number of arguments");
      return check_command(conn, words[1], command[5] == 'W');

  } else if (command == "LLRPC") {
    if (words.size() != 2 && words.size() != 3)
      return conn.message(CSERR_SYNTAX, "Wrong number of arguments");
    int datalen = 0, r;
    if (words.size() == 3 && (!IntArg().parse(words[2], datalen) || datalen < 0))
      return conn.message(CSERR_SYNTAX, "Syntax error in 'llrpc'");
    String data;
    if ((r = conn.read(datalen, data)) != 0)
	return r;
    return llrpc_command(conn, words[1], data);

  } else if (command == "CLOSE" || command == "QUIT") {
    if (words.size() != 1)
      conn.message(CSERR_SYNTAX, "Bad command syntax");
    conn.message(CSERR_OK, "Goodbye!");
    conn.in_closed = true;
    conn.in_text.clear();
    conn.inpos = 0;
    return 0;

  } else if (command == "HELP") {
    conn.message(CSERR_OK, "Commands supported:", true);
    conn.message(CSERR_OK, "READ handler [arg...]   call read handler, return DATA", true);
    conn.message(CSERR_OK, "READDATA handler len    call read handler with len data bytes, return DATA", true);
    conn.message(CSERR_OK, "READUNTIL handler term  call read handler, take data until term, return DATA", true);
    conn.message(CSERR_OK, "WRITE handler [arg...]  call write handler", true);
    conn.message(CSERR_OK, "WRITEDATA handler len   call write handler, pass len data bytes", true);
    conn.message(CSERR_OK, "WRITEUNTIL handler term call write handler, take data until term", true);
    conn.message(CSERR_OK, "CHECKREAD handler       check if read handler is valid", true);
    conn.message(CSERR_OK, "CHECKWRITE handler      check if write handler is valid", true);
    conn.message(CSERR_OK, "LLRPC elt#number [len]  call LLRPC, pass len data bytes, return DATA", true);
    conn.message(CSERR_OK, "QUIT                    close connection");
    return 0;

  } else
    return conn.message(CSERR_UNIMPLEMENTED, "Command '" + command + "' unimplemented");
}

void
ControlSocket::initialize_connection(int fd)
{
    fcntl(fd, F_SETFL, O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    add_select(fd, SELECT_READ | SELECT_WRITE);
    if (_conns.size() <= fd)
	_conns.resize(fd + 1);
    _conns[fd] = new connection(fd);
    _conns[fd]->out_text << "Click::ControlSocket/" << protocol_version << '\r' << '\n';
}

void
ControlSocket::selected(int fd, int)
{
    if (fd == _socket_fd) {
	union { struct sockaddr_in in; struct sockaddr_un un; } sa;
#if HAVE_ACCEPT_SOCKLEN_T
	socklen_t sa_len;
#else
	int sa_len;
#endif
	sa_len = sizeof(sa);
	int new_fd = accept(_socket_fd, (struct sockaddr *)&sa, &sa_len);

	if (new_fd < 0) {
	    if (errno != EAGAIN)
		click_chatter("%s: accept: %s", declaration().c_str(), strerror(errno));
	    return;
	}

	if (_verbose) {
	    if (_type == type_tcp)
		click_chatter("%s: opened connection %d from %s.%d", declaration().c_str(), new_fd, IPAddress(sa.in.sin_addr).unparse().c_str(), ntohs(sa.in.sin_port));
	    else
		click_chatter("%s: opened connection %d", declaration().c_str(), new_fd);
	}

	initialize_connection(new_fd);
	fd = new_fd;
    }

    // find file descriptor
    if (fd >= _conns.size() || !_conns[fd])
	return;
    connection *conn = _conns[fd];

    // read commands from socket (but only a bit on each select)
    if (!conn->in_closed)
	if (char *buf = conn->in_text.reserve(2048)) {
	    ssize_t r = read(conn->fd, buf, 2048);
	    if (r != 0 && r != -1)
		conn->in_text.adjust_length(r);
	    else if (r == 0 || (r == -1 && errno != EAGAIN && errno != EINTR))
		conn->in_closed = true;
	}

    // parse commands
    // 16.Jun.2004: process only one command each time through
    bool blocked = false;
    if (conn->in_text.length()) {
	const char *in_text = conn->in_text.begin() + conn->inpos;
	const char *in_end = conn->in_text.end();
	const char *line_end = in_text;
	while (line_end != in_end && *line_end != '\r' && *line_end != '\n')
	    ++line_end;
	if (line_end != in_end || conn->in_closed) {
	    // have a complete command, parse it

	    // include end of line
	    if (line_end + 2 <= in_end && *line_end == '\r' && line_end[1] == '\n')
		line_end += 2;
	    else if (line_end != in_end)
		++line_end;

	    // grab string
	    int oldpos = conn->inpos;
	    String line(in_text, line_end);
	    conn->inpos = line_end - conn->in_text.begin();

	    // parse each individual command
	    if (parse_command(*conn, line) > 0) {
		// more data to come, so wait
		conn->inpos = oldpos;
		blocked = true;
	    } else
		connection::contract(conn->in_text, conn->inpos);
	} else
	    // 12.Jul.2006, Cliff Frey: write incomplete, so we are blocked
	    blocked = true;
    }

    // write data until blocked
    // The 2nd argument causes write events to remain selected when commands
    // remain to be processed (whether or not CS has data to write).
    conn->flush_write(this, conn->in_text.length() && !blocked);

    // maybe close out
    if ((conn->in_closed && !conn->in_text.length() && !conn->out_text.length())
	|| conn->out_closed) {
	remove_select(conn->fd, SELECT_READ | SELECT_WRITE);
	close(conn->fd);
	if (_verbose)
	    click_chatter("%s: closed connection %d", declaration().c_str(), fd);
	_conns[conn->fd] = 0;
	delete conn;
    }
}

ErrorHandler *
ControlSocket::proxy_error_function(const String &h, void *thunk)
{
    ControlSocket *cs = static_cast<ControlSocket *>(thunk);
    return (h == cs->_proxied_handler ? cs->_proxied_errh : 0);
}

void
ControlSocket::add_handlers()
{
    if (_type == type_tcp)
	add_data_handlers("port", Handler::OP_READ, &_unix_pathname);
    else if (_type == type_unix)
	add_data_handlers("filename", Handler::OP_READ, &_unix_pathname);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ControlSocket)
