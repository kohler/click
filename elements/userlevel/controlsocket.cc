/*
 * controlsocket.{cc,hh} -- element listens to TCP/IP or Unix-domain sockets
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/package.hh>
#include "controlsocket.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>

const char *ControlSocket::protocol_version = "1.0";
enum {
  CSERR_OK			= 200,
  CSERR_OK_HANDLER_WARNING	= 220,
  CSERR_SYNTAX			= 500,
  CSERR_UNIMPLEMENTED		= 501,
  CSERR_NO_SUCH_ELEMENT		= 510,
  CSERR_NO_SUCH_HANDLER		= 511,
  CSERR_HANDLER_ERROR		= 520,
  CSERR_PERMISSION		= 530,
};

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
  _read_only = false;
  
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
		    cpBool, "read-only?", &_read_only,
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
		    cpBool, "read-only?", &_read_only,
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
  _out_texts[fd] += String(code) + (continuation ? "-" : " ") + s + "\r\n";
  return ANY_ERR;
}

int
ControlSocket::global_read_handler(int fd, const String &name, String *data)
{
  StringAccum sa;
  int ok = 0;
  if (!click_userlevel_global_handler_string(router(), name, data))
    ok = message(fd, CSERR_NO_SUCH_HANDLER, "No such handler `" + name + "'");
  return ok;
}

int
ControlSocket::parse_handler(int fd, const String &handlername, Element **es)
{
  int dot = handlername.find_left('.');
  if (dot < 0)
    return NAME_ERR;
  String element = handlername.substring(0, dot);
  String handler = handlername.substring(dot + 1);
  Element *e = router()->find(element);
  if (!e)
    return message(fd, CSERR_NO_SUCH_ELEMENT, "No element named `" + element + "'");
  int hid = router()->find_handler(e, handler);
  if (hid < 0)
    return message(fd, CSERR_NO_SUCH_HANDLER, "Element has no handler named `" + handler + "'");
  *es = e;
  return hid;
}

int
ControlSocket::write_command(int fd, const String &handlername, const String &data)
{
  Element *e;
  int hid = parse_handler(fd, handlername, &e);
  if (hid < 0) {
    if (hid == NAME_ERR)
      message(fd, CSERR_NO_SUCH_HANDLER, "No such handler `" + handlername + "'");
    return hid;
  }
  const Router::Handler &h = router()->handler(hid);
  if (!h.write)
    return message(fd, CSERR_NO_SUCH_HANDLER, "Handler `" + handlername + "' read-only");

  if (_read_only)
    return message(fd, CSERR_PERMISSION, "Permission denied `" + handlername + "'");
  
  // set up error handler
  ControlSocketErrorHandler errh;
  (void) /*XXX*/ h.write(data, e, h.write_thunk, &errh);

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
  
  Element *e;
  String command = words[0].upper();
  if (command == "READ" || command == "GET") {
    if (words.size() != 2)
      return message(fd, CSERR_SYNTAX, "Wrong number of arguments");
    String data;
    int hid = parse_handler(fd, words[1], &e);
    if (hid >= 0) {
      const Router::Handler &h = router()->handler(hid);
      if (!h.read)
	return message(fd, CSERR_NO_SUCH_HANDLER, "Handler `" + words[1] + "' write-only");
      data = h.read(e, h.read_thunk);
    } else if (hid != NAME_ERR || global_read_handler(fd, words[1], &data) < 0)
      return -1;
    message(fd, CSERR_OK, "Read handler `" + words[1] + "' OK");
    _out_texts[fd] += "DATA " + String(data.length()) + "\r\n";
    _out_texts[fd] += data;
    
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

  return 0;
}


void
ControlSocket::selected(int fd)
{
  if (fd == _socket_fd) {
    struct sockaddr_in sa;
    socklen_t sa_len = sizeof(sa);
    int new_fd = accept(_socket_fd, (struct sockaddr *)&sa, &sa_len);

    if (new_fd < 0) {
      if (errno != EAGAIN)
	click_chatter("%s: accept: %s", declaration().cc(), strerror(errno));
      return;
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
#if 0
    click_chatter("%s: closed %d", declaration().cc(), fd);
#endif
    _flags[fd] = -1;
  }
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ControlSocket)
