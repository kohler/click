/*
 * controlsocket.{cc,hh} -- element listens to TCP/IP or Unix-domain sockets
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/llrpc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>

const char * const ControlSocket::protocol_version = "1.0";

struct ControlSocketErrorHandler : public ErrorHandler {

  Vector<String> _messages;
  int _nwarnings;
  int _nerrors;

 public:

  ControlSocketErrorHandler()		{ reset_counts(); }

  int nwarnings() const			{ return _nwarnings; }
  int nerrors() const			{ return _nerrors; }
  void reset_counts()			{ _nwarnings = _nerrors = 0; }
  const Vector<String> &messages() const { return _messages; }
  
  void handle_text(Seriousness, const String &);
  
};

void
ControlSocketErrorHandler::handle_text(Seriousness seriousness, const String &m)
{
  switch (seriousness) {
   case ERR_WARNING:			_nwarnings++; break;
   case ERR_ERROR: case ERR_FATAL:	_nerrors++; break;
   default:				break;
  }
  int pos = 0, nl;
  while ((nl = m.find_left('\n', pos)) >= 0) {
    _messages.push_back(m.substring(pos, nl - pos));
    pos = nl + 1;
  }
  if (pos < m.length())
    _messages.push_back(m.substring(pos));
}


ControlSocket::ControlSocket()
{
  MOD_INC_USE_COUNT;
}

ControlSocket::~ControlSocket()
{
  MOD_DEC_USE_COUNT;
}

int
ControlSocket::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _socket_fd = -1;
  bool read_only = false, verbose = false;
  _proxy = 0;
  
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket (`TCP' or `UNIX')", &socktype,
		  cpIgnoreRest, cpEnd) < 0)
    return -1;

  socktype = socktype.upper();
  if (socktype == "TCP") {
    unsigned short portno;
    if (cp_va_parse(conf, this, errh,
		    cpIgnore,
		    cpUnsignedShort, "port number", &portno,
		    cpOptional,
		    cpBool, "read-only?", &read_only,
		    cpKeywords,
		    "READONLY", cpBool, "read-only?", &read_only,
		    "PROXY", cpElement, "handler proxy", &_proxy,
		    "VERBOSE", cpBool, "be verbose?", &verbose,
		    cpEnd) < 0)
      return -1;

    // open socket, set options
    _socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_socket_fd < 0)
      return errh->error("socket: %s", strerror(errno));
    int sockopt = 1;
    if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&sockopt, sizeof(sockopt)) < 0)
      errh->warning("setsockopt: %s", strerror(errno));

    // bind to port
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portno);
    sa.sin_addr = inet_makeaddr(0, 0);
    if (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
      uninitialize();
      return errh->error("bind: %s", strerror(errno));
    }

  } else if (socktype == "UNIX") {
    if (cp_va_parse(conf, this, errh,
		    cpIgnore,
		    cpString, "filename", &_unix_pathname,
		    cpOptional,
		    cpBool, "read-only?", &read_only,
		    cpKeywords,
		    "READONLY", cpBool, "read-only?", &read_only,
		    "PROXY", cpElement, "handler proxy", &_proxy,
		    "VERBOSE", cpBool, "be verbose?", &verbose,
		    cpEnd) < 0)
      return -1;

    // create socket address
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    if (_unix_pathname.length() >= (int)sizeof(sa.sun_path))
      return errh->error("filename too long");
    memcpy(sa.sun_path, _unix_pathname.cc(), _unix_pathname.length() + 1);
    
    // open socket, set options
    _socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (_socket_fd < 0)
      return errh->error("socket: %s", strerror(errno));

    // bind to port
    if (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
      uninitialize();
      return errh->error("bind: %s", strerror(errno));
    }

  } else
    return errh->error("unknown socket type `%s'", socktype.cc());

  _read_only = read_only;
  _verbose = verbose;
  return 0;
}

int
ControlSocket::initialize(ErrorHandler *errh)
{
  // start listening
  if (listen(_socket_fd, 2) < 0) {
    uninitialize();
    return errh->error("listen: %s", strerror(errno));
  }
  
  // nonblocking I/O on the socket
  fcntl(_socket_fd, F_SETFL, O_NONBLOCK);

  // ask the proxy to send us errors
  if (_proxy) {
    struct {
      void (*function)(const String &, int, void *);
      void *thunk;
    } x;
    x.function = proxy_error_function;
    x.thunk = this;
    _proxy->local_llrpc(CLICK_LLRPC_ADD_HANDLER_ERROR_PROXY, &x);
  }

  add_select(_socket_fd, SELECT_READ | SELECT_WRITE);
  return 0;
}

void
ControlSocket::uninitialize()
{
  if (_socket_fd >= 0) {
    close(_socket_fd);
    if (_unix_pathname)
      unlink(_unix_pathname);
  }
  _socket_fd = -1;
  for (int i = 0; i < _flags.size(); i++)
    if (_flags[i] >= 0) {
      close(i);
      _flags[i] = -1;
    }
}

int
ControlSocket::message(int fd, int code, const String &s, bool continuation)
{
  assert(code >= 100 && code <= 999);
  _out_texts[fd] += String(code) + (continuation ? "-" : " ") + s.printable() + "\r\n";
  return ANY_ERR;
}

int
ControlSocket::parse_handler(int fd, const String &full_name, Element **es)
{
  int dot = full_name.find_left('.');
  
  if (_proxy) {
    String new_name = (dot < 0 ? "0." + full_name : full_name);
    int hid = router()->find_handler(_proxy, new_name);
    if (hid < 0)
      return message(fd, CSERR_NO_SUCH_HANDLER, "No proxied handler named `" + full_name + "'");
    *es = _proxy;
    return hid;
  } else if (dot < 0) {
    int hid = router()->find_handler(0, full_name);
    if (hid < 0)
      return message(fd, CSERR_NO_SUCH_HANDLER, "No global handler named `" + full_name + "'");
    *es = 0;
    return hid;
  } else {
    String element = full_name.substring(0, dot);
    String handler = full_name.substring(dot + 1);
    Element *e = (_proxy ? _proxy : router()->find(element));
    if (!e)
      return message(fd, CSERR_NO_SUCH_ELEMENT, "No element named `" + element + "'");
    int hid = router()->find_handler(e, handler);
    if (hid < 0)
      return message(fd, CSERR_NO_SUCH_HANDLER, "No handler named `" + full_name + "'");
    *es = e;
    return hid;
  }
}

int
ControlSocket::report_proxy_errors(int fd, const String &handlername)
{
  String proxy_name = handlername;
  if (handlername.find_left('.') < 0) // prepend "0."
    proxy_name = "0." + handlername;
  
  int num = 0;
  for (int i = 0; i < _herror_handlers.size(); i++)
    if (_herror_handlers[i] == proxy_name) {
      switch (_herror_messages[i]) {
	
       case CSERR_NO_SUCH_ELEMENT: {
	 int dot = handlername.find_left('.');
	 assert(dot >= 0);
	 message(fd, CSERR_NO_SUCH_ELEMENT, "No element named `" + handlername.substring(0, dot) + "'");
	 break;
       }
       
       case CSERR_NO_SUCH_HANDLER:
	message(fd, CSERR_NO_SUCH_HANDLER, "No handler named `" + handlername + "'");
	break;
	
       case CSERR_PERMISSION:
	message(fd, CSERR_PERMISSION, "Permission denied for `" + handlername + "'");
	break;
	
       case CSERR_UNSPECIFIED:
       default:
	message(fd, _herror_messages[i], "Error accessing handler `" + handlername + "'");
	break;
	
      }
      num++;
    }

  return -num;
}

int
ControlSocket::read_command(int fd, const String &handlername)
{
  Element *e;
  int hid = parse_handler(fd, handlername, &e);
  if (hid < 0)
    return hid;
  const Router::Handler &h = router()->handler(hid);
  if (!h.read)
    return message(fd, CSERR_NO_SUCH_HANDLER, "Handler `" + handlername + "' write-only");
  
  String data = h.read(e, h.read_thunk);

  // check for error messages from proxy
  if (_proxy && report_proxy_errors(fd, handlername) < 0)
    return -1;
  
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
  const Router::Handler &h = router()->handler(hid);
  if (!h.write)
    return message(fd, CSERR_NO_SUCH_HANDLER, "Handler `" + handlername + "' read-only");

  if (_read_only)
    return message(fd, CSERR_PERMISSION, "Permission denied for `" + handlername + "'");
  
  // call handler
  ControlSocketErrorHandler errh;
  int result = h.write(data, e, h.write_thunk, &errh);

  // check for proxy-related errors
  if (_proxy && result < 0 && report_proxy_errors(fd, handlername) < 0)
    return -1;

  int code = CSERR_OK;
  String happened = "OK";
  if (errh.nerrors() > 0)
    code = CSERR_HANDLER_ERROR, happened = "error";
  else if (errh.nwarnings() > 0)
    code = CSERR_OK_HANDLER_WARNING, happened = "OK with warnings";

  const Vector<String> &messages = errh.messages();
  if (messages.size() > 0)
    happened += ":";
  message(fd, code, "Write handler `" + handlername + "' " + happened, messages.size() > 0);
  for (int i = 0; i < messages.size(); i++)
    message(fd, code, messages[i], i < messages.size() - 1);
  
  return 0;
}

int
ControlSocket::parse_command(int fd, const String &line)
{
  Vector<String> words;
  cp_spacevec(line, words);
  if (words.size() == 0)
    return 0;
  
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
ControlSocket::selected(int fd)
{
  if (fd == _socket_fd) {
    union { struct sockaddr_in in; struct sockaddr_un un; } sa;
    socklen_t sa_len = sizeof(sa);
    int new_fd = accept(_socket_fd, (struct sockaddr *)&sa, &sa_len);

    if (new_fd < 0) {
      if (errno != EAGAIN)
	click_chatter("%s: accept: %s", declaration().cc(), strerror(errno));
      return;
    }

    if (_verbose) {
      if (!_unix_pathname)
	click_chatter("%s: opened connection %d from %s.%d", declaration().cc(), new_fd, IPAddress(sa.in.sin_addr).unparse().cc(), ntohs(sa.in.sin_port));
      else
	click_chatter("%s: opened connection %d", declaration().cc(), new_fd);
    }

    fcntl(new_fd, F_SETFL, O_NONBLOCK);
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

    // clear out proxy errors
    _herror_handlers.clear();
    _herror_messages.clear();
    
    // parse each individual command
    if (parse_command(fd, line) > 0) {
      // more data to come, so wait
      _in_texts[fd] = old_text;
      break;
    }
  }

  // write data until blocked
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
  }

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


void
ControlSocket::proxy_error_function(const String &h, int err, void *thunk)
{
  ControlSocket *cs = (ControlSocket *)thunk;
  cs->_herror_handlers.push_back(h);
  cs->_herror_messages.push_back(err);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ControlSocket)
