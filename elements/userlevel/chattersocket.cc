/*
 * chattersocket.{cc,hh} -- element echoes chatter to TCP/IP or Unix-domain
 * sockets
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 ACIRI
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
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html */

#include <click/config.h>
#include <click/package.hh>
#include "chattersocket.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/click_tcp.h>	/* for SEQ_LT, etc. */
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>

const char *ChatterSocket::protocol_version = "1.0";

struct ChatterSocketErrorHandler : public ErrorVeneer {

  Vector<ChatterSocket *> _chatter_sockets;

 public:

  ChatterSocketErrorHandler(ErrorHandler *errh)	: ErrorVeneer(errh) { }

  ErrorHandler *base_errh() const	{ return _errh; }
  int nchatter_sockets() const		{ return _chatter_sockets.size(); }
  
  void add_chatter_socket(ChatterSocket *);
  void remove_chatter_socket(ChatterSocket *);
  
  void handle_text(Seriousness, const String &);
  
};

void
ChatterSocketErrorHandler::add_chatter_socket(ChatterSocket *cs)
{
  for (int i = 0; i < _chatter_sockets.size(); i++)
    if (_chatter_sockets[i] == cs)
      return;
  _chatter_sockets.push_back(cs);
}

void
ChatterSocketErrorHandler::remove_chatter_socket(ChatterSocket *cs)
{
  for (int i = 0; i < _chatter_sockets.size(); i++)
    if (_chatter_sockets[i] == cs) {
      _chatter_sockets[i] = _chatter_sockets.back();
      _chatter_sockets.pop_back();
      return;
    }
}

void
ChatterSocketErrorHandler::handle_text(Seriousness seriousness, const String &m)
{
  String actual_m = m;
  if (m.length() > 0 && m.back() != '\n')
    actual_m += '\n';
  _errh->handle_text(seriousness, actual_m);
  for (int i = 0; i < _chatter_sockets.size(); i++)
    _chatter_sockets[i]->handle_text(seriousness, actual_m);
}


static ChatterSocketErrorHandler *chatter_socket_errh;

ChatterSocket::ChatterSocket()
{
  MOD_INC_USE_COUNT;
}

ChatterSocket::~ChatterSocket()
{
  MOD_DEC_USE_COUNT;
}

int
ChatterSocket::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _socket_fd = -1;
  
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
ChatterSocket::initialize(ErrorHandler *errh)
{
  // start listening
  if (listen(_socket_fd, 2) < 0) {
    uninitialize();
    return errh->error("listen: %s", strerror(errno));
  }
  
  // nonblocking I/O on the socket
  fcntl(_socket_fd, F_SETFL, O_NONBLOCK);

  // select
  add_select(_socket_fd, SELECT_READ | SELECT_WRITE);
  _max_pos = 0;
  _live_fds = 0;

  // install ChatterSocketErrorHandler
  if (!chatter_socket_errh) {
    chatter_socket_errh = new ChatterSocketErrorHandler(ErrorHandler::default_handler());
    ErrorHandler::set_default_handler(chatter_socket_errh);
  }
  chatter_socket_errh->add_chatter_socket(this);
  
  return 0;
}

void
ChatterSocket::uninitialize()
{
  if (_socket_fd >= 0) {
    close(_socket_fd);
    if (_unix_pathname)
      unlink(_unix_pathname);
  }
  _socket_fd = -1;
  for (int i = 0; i < _fd_alive.size(); i++)
    if (_fd_alive[i]) {
      close(i);
      _fd_alive[i] = 0;
    }
  _live_fds = 0;
  if (chatter_socket_errh) {
    chatter_socket_errh->remove_chatter_socket(this);
    if (!chatter_socket_errh->nchatter_sockets()) {
      ErrorHandler::set_default_handler(chatter_socket_errh->base_errh());
      delete chatter_socket_errh;
      chatter_socket_errh = 0;
    }
  }
}

int
ChatterSocket::write_chatter(int fd, int min_useful_message)
{
  // check file descriptor
  if (fd >= _fd_alive.size() || !_fd_alive[fd])
    return min_useful_message;

  // check if all data written
  if (_fd_pos[fd] == _max_pos)
    return min_useful_message;

  // find first useful message
  int fd_pos = _fd_pos[fd];
  int mid = _message_pos.size() - 1;
  while (SEQ_LT(fd_pos, _message_pos[mid]))
    mid--;
  
  // write data until blocked or closed
  int w = 0;
  while (mid < _message_pos.size()) {
    const String &m = _messages[mid];
    int mpos = _message_pos[mid];
    const char *data = m.data() + (fd_pos - mpos);
    int len = m.length() - (fd_pos - mpos);
    w = write(fd, data, len);
    if (w < 0 && errno != EINTR)
      break;
    if (w > 0)
      fd_pos += len;
    if (SEQ_GEQ(fd_pos, mpos + m.length()))
      mid++;
  }

  // store changed fd_pos
  _fd_pos[fd] = fd_pos;
  
  // maybe close out
  if (w < 0 && errno == EPIPE) {
    close(fd);
    remove_select(fd, SELECT_WRITE);
    _fd_alive[fd] = 0;
    _live_fds--;
    return min_useful_message;
  } else
    return (mid < min_useful_message ? mid : min_useful_message);
}

void
ChatterSocket::write_chatter()
{
  int min_useful_message = _messages.size();
  if (min_useful_message)
    for (int i = 0; i < _fd_alive.size(); i++)
      if (_fd_alive[i] >= 0)
	min_useful_message = write_chatter(i, min_useful_message);

  // cull old messages
  if (min_useful_message >= 10) {
    for (int i = min_useful_message; i < _messages.size(); i++) {
      _messages[i - min_useful_message] = _messages[i];
      _message_pos[i - min_useful_message] = _message_pos[i];
    }
    _messages.resize(_messages.size() - min_useful_message);
    _message_pos.resize(_message_pos.size() - min_useful_message);
  }
}

void
ChatterSocket::selected(int fd)
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
    add_select(new_fd, SELECT_WRITE);

    while (new_fd >= _fd_alive.size()) {
      _fd_alive.push_back(0);
      _fd_pos.push_back(0);
    }
    _fd_alive[new_fd] = 1;
    _fd_pos[new_fd] = _max_pos;
    _live_fds++;

    fd = new_fd;

    // XXX - assume that this write will succeed
    String s = String("Click::ChatterSocket/") + protocol_version + "\r\n";
    int w = write(fd, s.data(), s.length());
    if (w != s.length())
      click_chatter("%s fd %d: unable to write banner!", declaration().cc(), fd);
  }

  write_chatter(fd, 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ChatterSocket)
