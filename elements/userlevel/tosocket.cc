// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * tosocket.{cc,hh} -- element write data to socket
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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include "tosocket.hh"

CLICK_DECLS

ToSocket::ToSocket()
  : Element(1, 0), _task(this), _verbose(false), _fd(-1), _active(-1),
    _snaplen(2048), _nodelay(1), _frame(true)
{
  MOD_INC_USE_COUNT;
}

ToSocket::~ToSocket()
{
  MOD_DEC_USE_COUNT;
}

void
ToSocket::notify_noutputs(int n)
{
  set_noutputs(n <= 1 ? n : 1);
}

int
ToSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket (`TCP' or `UDP' or `UNIX')", &socktype,
		  cpIgnoreRest,
		  cpEnd) < 0)
    return -1;
  socktype = socktype.upper();

  // remove keyword arguments
  if (cp_va_parse_remove_keywords(conf, 2, this, errh,
		"VERBOSE", cpBool, "be verbose?", &_verbose,
		"SNAPLEN", cpUnsigned, "maximum packet length", &_snaplen,
		"NODELAY", cpUnsigned, "disable Nagle algorithm?", &_nodelay,
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
		    cpIgnore,
		    cpString, "filename", &_pathname,
		    cpEnd) < 0)
      return -1;
    if (_pathname.length() >= (int)sizeof(((struct sockaddr_un *)0)->sun_path))
      return errh->error("filename too long");
  }

  else
    return errh->error("unknown socket type `%s'", socktype.cc());

  return 0;
}


int
ToSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_fd >= 0) {
    close(_fd);
    _fd = -1;
  }

  return errh->error("%s: %s", syscall, strerror(e));
}

int
ToSocket::initialize(ErrorHandler *errh)
{
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

#ifdef SO_SNDBUF
  // set transmit buffer size
  if (setsockopt(_fd, SOL_SOCKET, SO_SNDBUF, &_snaplen, sizeof(_snaplen)) < 0)
    return initialize_socket_error(errh, "setsockopt");
#endif

#ifdef TCP_NODELAY
  // disable Nagle algorithm
  if (_protocol == IPPROTO_TCP && _nodelay)
    if (setsockopt(_fd, IP_PROTO_TCP, TCP_NODELAY, &_nodelay, sizeof(_nodelay)) < 0)
      return initialize_socket_error(errh, "setsockopt");
#endif

  if (_protocol == IPPROTO_TCP && !_ip) {
    // bind to port
    if (bind(_fd, (struct sockaddr *)&sa, sa_len) < 0)
      return initialize_socket_error(errh, "bind");

    // start listening
    if (_socktype == SOCK_STREAM)
      if (listen(_fd, 0) < 0)
	return initialize_socket_error(errh, "listen");

    add_select(_fd, SELECT_READ);
  }
  else {
    // connect
    if (_socktype == SOCK_STREAM)
      if (connect(_fd, (struct sockaddr *)&sa, sa_len) < 0)
	return initialize_socket_error(errh, "connect");

    _active = _fd;
  }

  if (input_is_pull(0)) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
  }
  return 0;
}

void
ToSocket::cleanup(CleanupStage)
{
  if (_active >= 0 && _active != _fd) {
    close(_active);
    _active = -1;
  }
  if (_fd >= 0) {
    // shut down the listening socket in case we forked
#ifdef SHUT_RDWR
    shutdown(_fd, SHUT_RDWR);
#else
    shutdown(_fd, 2);
#endif
    close(_fd);
    _fd = -1;
  }
}

void
ToSocket::selected(int fd)
{
  int new_fd = accept(fd, (struct sockaddr *)&sa, &sa_len);

  if (new_fd < 0) {
    if (errno != EAGAIN)
      click_chatter("%s: accept: %s", declaration().cc(), strerror(errno));
    return;
  }

  if (_verbose) {
    if (_family == PF_INET)
      click_chatter("%s: opened connection %d from %s.%d", declaration().cc(), new_fd, IPAddress(sa.in.sin_addr).unparse().cc(), ntohs(sa.in.sin_port));
    else
      click_chatter("%s: opened connection %d", declaration().cc(), new_fd);
  }

  _active = new_fd;
}

void
ToSocket::send_packet(Packet *p)
{
  int w = 0;
  if (_frame) {
    p->push(sizeof(uint32_t));
    *(uint32_t *)p->data() = htonl(p->length());
  }
  while (p->length()) {
    w = sendto(_active, p->data(), p->length(), 0, (const struct sockaddr*)&sa, sa_len);
    if (w < 0 && errno != EINTR)
      break;
    p->pull(w);
  }
  if (w < 0) {
    click_chatter("ToSocket: sendto: %s", strerror(errno));
    checked_output_push(0, p);
  }
  else
    p->kill();
}

void
ToSocket::push(int, Packet *p)
{
  send_packet(p);
}

bool
ToSocket::run_task()
{
  // XXX reduce tickets when idle
  Packet *p = input(0).pull();
  if (p)
    send_packet(p);
  else if (!_signal)
    return false;
  _task.fast_reschedule();
  return p != 0;
}

void
ToSocket::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ToSocket)
