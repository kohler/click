// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * fromsocket.{cc,hh} -- element reads socket data
 * Mark Huang <mlhuang@cs.princeton.edu>
 *
 * Copyright (c) 2004  The Trustees of Princeton University (Trustees).
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
#include "fromsocket.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>

CLICK_DECLS

FromSocket::FromSocket()
  : Element(0, 1), _verbose(false), _fd(-1),
    _snaplen(2048), _frame(true)
{
}

FromSocket::~FromSocket()
{
}

int
FromSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket ('TCP' or 'UDP' or 'UNIX')", &socktype,
		  cpIgnoreRest, cpEnd) < 0)
    return -1;
  socktype = socktype.upper();

  // remove keyword arguments
  if (cp_va_parse_remove_keywords(conf, 1, this, errh,
		"VERBOSE", cpBool, "be verbose?", &_verbose,
		"SNAPLEN", cpUnsigned, "maximum packet length", &_snaplen,
		"FRAME", cpBool, "frame packets?", &_frame,
		cpEnd) < 0)
    return -1;

  if (socktype == "TCP" || socktype == "UDP") {
    _family = PF_INET;
    _socktype = socktype == "TCP" ? SOCK_STREAM : SOCK_DGRAM;
    _protocol = socktype == "TCP" ? IPPROTO_TCP : IPPROTO_UDP;
    if (cp_va_parse(conf, this, errh,
		    cpIgnore,
		    cpIPAddress, "IP address", &_ip,
		    cpUnsignedShort, "port number", &_port,
		    cpEnd) < 0)
      return -1;
  }

  else if (socktype == "UNIX") {
    _family = PF_UNIX;
    _socktype = SOCK_STREAM;
    _protocol = 0;
    if (cp_va_parse(conf, this, errh,
		    cpIgnore, cpString, "filename", &_pathname,
		    cpEnd) < 0)
      return -1;
    if (_pathname.length() >= (int)sizeof(((struct sockaddr_un *)0)->sun_path))
      return errh->error("filename too long");
  }

  else
    return errh->error("unknown socket type '%s'", socktype.cc());

  return 0;
}


int
FromSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_fd >= 0) {
    close(_fd);
    _fd = -1;
  }

  return errh->error("%s: %s", syscall, strerror(e));
}

int
FromSocket::initialize(ErrorHandler *errh)
{
  union { struct sockaddr_in in; struct sockaddr_un un; } sa;
  socklen_t sa_len;

  // open socket, set options, bind to address
  _fd = socket(_family, _socktype, _protocol);
  if (_fd < 0)
    return initialize_socket_error(errh, "socket");

  if (_family == PF_INET) {
    sa.in.sin_family = _family;
    sa.in.sin_port = htons(_port);
    sa.in.sin_addr = _ip.in_addr();
    sa_len = sizeof(sa.in);
  }
  else {
    sa.un.sun_family = _family;
    strcpy(sa.un.sun_path, _pathname.cc());
    sa_len = sizeof(sa.un.sun_family) + _pathname.length();
  }

#ifdef SO_RCVBUF
  // set receive buffer size
  if (setsockopt(_fd, SOL_SOCKET, SO_RCVBUF, &_snaplen, sizeof(_snaplen)) < 0)
    return initialize_socket_error(errh, "setsockopt");
#endif

  if (_protocol == IPPROTO_TCP && _ip) {
    // connect
    if (_socktype == SOCK_STREAM) {
      if (connect(_fd, (struct sockaddr *)&sa, sa_len) < 0)
	return initialize_socket_error(errh, "connect");
      if (_verbose)
	click_chatter("%s: opened connection %d to %s:%d", declaration().cc(), _fd, IPAddress(sa.in.sin_addr).unparse().cc(), ntohs(sa.in.sin_port));
    }
  }
  else {
    // bind to port
    if (bind(_fd, (struct sockaddr *)&sa, sa_len) < 0)
      return initialize_socket_error(errh, "bind");

    // start listening
    if (_socktype == SOCK_STREAM)
      if (listen(_fd, 2) < 0)
	return initialize_socket_error(errh, "listen");
  }

  // nonblocking I/O and close-on-exec for the socket
  fcntl(_fd, F_SETFL, O_NONBLOCK);
  fcntl(_fd, F_SETFD, FD_CLOEXEC);

  add_select(_fd, SELECT_READ);
  return 0;
}

void
FromSocket::cleanup(CleanupStage)
{
  if (_fd >= 0) {
    // shut down the listening socket in case we forked
#ifdef SHUT_RDWR
    shutdown(_fd, SHUT_RDWR);
#else
    shutdown(_fd, 2);
#endif
    close(_fd);
    remove_select(_fd, SELECT_READ);
    if (_family == PF_UNIX)
      unlink(_pathname.c_str());
    _fd = -1;
  }
  for (int i = 0; i < _active.size(); i++) {
    if (_active[i]) {
      close(i);
      remove_select(i, SELECT_READ);
      _active[i] = 0;
    }
    if (_packets[i]) {
      _packets[i]->kill();
      _packets[i] = NULL;
    }
  }
}

void *
FromSocket::handle(int fd)
{
  assert(_active[fd]);

  // allocate packet
  if (!_packets[fd]) {
    _packets[fd] = Packet::make(_snaplen);
    assert(_packets[fd]);
    _packets[fd]->take(_snaplen);
  }

  // read data from socket
  WritablePacket *p = _packets[fd];
  _packets[fd] = NULL;
  assert(p);

  // read data from socket
  int len = read(fd, p->end_data(), p->tailroom());

  if (len > 0) {
    p = p->put(len);
    if (!p)
      return NULL;
  } else {
    p->kill();
    if (len <= 0 && errno != EAGAIN) {
      click_chatter("%s: read: %s", declaration().cc(), strerror(errno));
      goto err;
    }
    return NULL;
  }

  // check framing mark
  if (_frame && p->length() >= 4) {
    unsigned length = ntohl(*(uint32_t *)p->data());
    if (length < 4 || length > _snaplen) {
      click_chatter("%s: bad framing mark %08x", declaration().cc(), length);
      goto err;
    }
    else if (p->length() > length) {
      // trim this packet
      unsigned extra = p->length() - length;
      p->take(extra);
      // allocate next packet
      assert(!_packets[fd]);
      _packets[fd] = Packet::make(_snaplen);
      assert(_packets[fd]);
      // fill next packet
      memcpy(_packets[fd]->data(), p->end_data(), extra);
      _packets[fd]->take(_snaplen - extra);
    }
    else if (p->length() < length) {
      // hold out for more data
      _packets[fd] = p;
      return NULL;
    }
    // pull framing mark
    p->pull(4);
  }

  // set timestamp
  p->timestamp_anno().set_now();

  if (p->length())
    output(0).push(p);
  else
    p->kill();

  return _packets[fd];

 err:
  assert(p && p != _packets[fd]);
  p->kill();
  close(fd);
  _active[fd] = 0;
  assert(!_packets[fd]);
  if (_verbose)
    click_chatter("%s: closed connection %d", declaration().cc(), fd);
  return NULL;
}

void
FromSocket::selected(int fd)
{
  union { struct sockaddr_in in; struct sockaddr_un un; } sa;
  socklen_t sa_len = sizeof(sa);

  if (fd == _fd) {
    if (_socktype == SOCK_STREAM && !_ip) {
      int new_fd = accept(_fd, (struct sockaddr *)&sa, &sa_len);

      if (new_fd < 0) {
	if (errno != EAGAIN)
	  click_chatter("%s: accept: %s", declaration().cc(), strerror(errno));
	return;
      }

      if (_verbose) {
	if (_family == PF_INET)
	  click_chatter("%s: opened connection %d from %s:%d", declaration().cc(), new_fd, IPAddress(sa.in.sin_addr).unparse().cc(), ntohs(sa.in.sin_port));
	else
	  click_chatter("%s: opened connection %d", declaration().cc(), new_fd);
      }

      fcntl(new_fd, F_SETFL, O_NONBLOCK);
      fcntl(new_fd, F_SETFD, FD_CLOEXEC);
      add_select(new_fd, SELECT_READ);

      fd = new_fd;
    }
  }

  while (fd >= _active.size()) {
    _active.push_back(0);
    _packets.push_back(NULL);
  }
  if (!_active[fd]) {
    _active[fd] = 1;
    assert(!_packets[fd]);
  }

  while (handle(fd));
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromSocket)
