/*
 * controlsocket.{cc,hh} -- element listens to TCP/IP or Unix-domain sockets
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "controlsocket.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/netinet.h>
#include <arpa/inet.h>
#include <fcntl.h>

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
  
  void vmessage(Seriousness, const String &);
  
};

void
ControlSocketErrorHandler::vmessage(Seriousness seriousness, const String &m)
{
  switch (seriousness) {
   case Warning: _nwarnings++; break;
   case Error: case Fatal: _nerrors++; break;
   default: break;
  }
  int pos = 0;
  while (pos < m.length()) {
    int next = m.find_left('\n', pos);
    if (next < 0) break;
    _messages.push_back(m.substring(pos, next - pos));
    pos = next + 1;
  }
  if (pos < m.length())
    _messages.push_back(m.substring(pos));
}


int
ControlSocket::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _socket_fd = -1;
  
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket (`TCP' or `UNIX')", &socktype,
		  cpIgnoreRest, cpEnd) < 0)
    return -1;
  
  if (socktype == "TCP") {
    unsigned portno;
    if (cp_va_parse(conf, this, errh,
		    cpIgnore,
		    cpUnsigned, "port number", &portno,
		    cpEnd) < 0)
      return -1;
    if (portno > 65535)
      return errh->error("port number out of range");

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

    // start listening
    if (listen(_socket_fd, 2) < 0) {
      uninitialize();
      return errh->error("listen: %s", strerror(errno));
    }
    
    // nonblocking I/O on the socket
    fcntl(_socket_fd, F_SETFL, O_NONBLOCK);
    
  } else if (socktype == "UNIX") {
    if (cp_va_parse(conf, this, errh,
		    cpIgnore,
		    cpString, "filename", &_unix_pathname,
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

    // start listening
    if (listen(_socket_fd, 2) < 0) {
      uninitialize();
      return errh->error("listen: %s", strerror(errno));
    }
    
    // nonblocking I/O on the socket
    fcntl(_socket_fd, F_SETFL, O_NONBLOCK);

  } else
    return errh->error("unknown socket type `%s'", socktype.cc());
  
  return 0;
}

int
ControlSocket::initialize(ErrorHandler *)
{
  add_select(_socket_fd);
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
  return -1;
}

int
ControlSocket::parse_handler(int fd, const String &handlername, Element **es)
{
  int dot = handlername.find_left('.');
  if (dot < 0)
    return message(fd, 999, "Bad handler name");
  String element = handlername.substring(0, dot);
  String handler = handlername.substring(dot + 1);
  Element *e = router()->find(element);
  if (!e)
    return message(fd, 999, "Bad element name");
  int hid = router()->find_handler(e, handler);
  if (hid < 0)
    return message(fd, 999, "No such handler");
  *es = e;
  return hid;
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
    return message(fd, 999, "Not a read handler");
  
  // set up error handler
  ControlSocketErrorHandler errh;
  (void) /*XXX*/ h.write(data, e, h.write_thunk, &errh);

  int code = 200;
  String happened = "OK";
  if (errh.nerrors() > 0)
    code = 290, happened = "error";
  else if (errh.nwarnings() > 0)
    code = 240;

  String context_string = "Write handler `" + h.name + "' ";
  if (e) context_string += String("for `") + e->declaration() + "' ";
  message(fd, code, context_string + happened);
  const Vector<String> &messages = errh.messages();
  for (int i = 0; i < messages.size(); i++)
    message(fd, code, messages[i], true);
  
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
  if (words[0] == "read" || words[0] == "get") {
    if (words.size() != 2)
      return message(fd, 999, "Bad command syntax");
    int hid = parse_handler(fd, words[1], &e);
    if (hid < 0) return hid;
    const Router::Handler &h = router()->handler(hid);
    if (!h.read)
      return message(fd, 999, "Not a read handler");
    String data = h.read(e, h.read_thunk);
    message(fd, 200, "OK " + String(data.length()));
    _out_texts[fd] += data;
    
  } else if (words[0] == "write") {
    if (words.size() < 2)
      return message(fd, 999, "Bad command syntax");
    String data;
    for (int i = 2; i < words.size(); i++)
      data += (i == 2 ? "" : " ") + words[i];
    return write_command(fd, words[1], data);
    
  } else if (words[0] == "writex") {
    if (words.size() != 3)
      return message(fd, 999, "Bad command syntax");
    int datalen;
    if (!cp_integer(words[2], &datalen) || datalen < 0)
      return message(fd, 999, "writex bad command syntax");
    if (_in_texts[fd].length() < datalen) {
      if (_flags[fd] & READ_CLOSED)
	return message(fd, 999, "Not enough data");
      else			// retry
	return 1;
    }
    String data = _in_texts[fd].substring(0, datalen);
    _in_texts[fd] = _in_texts[fd].substring(datalen);
    return write_command(fd, words[1], data);

  } else if (words[0] == "close") {
    if (words.size() != 1)
      message(fd, 999, "Bad command syntax");
    message(fd, 200, "Goodbye!");
    _flags[fd] |= READ_CLOSED;
    _in_texts[fd] = String();
    return 0;
    
  } else
    return message(fd, 999, "Bad command " + words[0]);

  return 0;
}


void
ControlSocket::selected(int fd)
{
  if (fd == _socket_fd) {
    struct sockaddr_in sa;
    socklen_t sa_len;
    int new_fd = accept(_socket_fd, (struct sockaddr *)&sa, &sa_len);

    if (new_fd < 0) {
      if (errno != EAGAIN)
	click_chatter("%s: accept: %s", declaration().cc(), strerror(errno));
      return;
    }

    { unsigned x = sa.sin_addr.s_addr;  click_chatter("%s: %d.%d.%d.%d:%d -> %d", declaration().cc(), (int)(x>>24)&255, (int)(x>>16)&255, (int)(x>>8)&255, x&255, sa.sin_port, new_fd);}
    
    fcntl(new_fd, F_SETFL, O_NONBLOCK);
    add_select(new_fd);

    while (new_fd >= _in_texts.size()) {
      _in_texts.push_back(String());
      _out_texts.push_back(String());
      _flags.push_back(-1);
    }
    _in_texts[new_fd] = String();
    _out_texts[new_fd] = String();
    _flags[new_fd] = 0;

    fd = new_fd;
    message(fd, 200, "Hello!");
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

    // grab string
    if (pos < len) pos++;
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
    remove_select(fd);
    click_chatter("%s: closed %d", declaration().cc(), fd);
    _flags[fd] = -1;
  }
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ControlSocket)
