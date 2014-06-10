// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * socket.{cc,hh} -- transports packets via sockets
 * Mark Huang <mlhuang@cs.princeton.edu>
 *
 * Copyright (c) 2004  The Trustees of Princeton University (Trustees).
 * Copyright (c) 2006-2007 Regents of the University of California
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
#include <click/args.hh>
#include <click/glue.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include "socket.hh"

#ifdef HAVE_PROPER
#include <proper/prop.h>
#endif

CLICK_DECLS

Socket::Socket()
  : _task(this),
    _fd(-1), _active(-1), _rq(0), _wq(0),
    _local_port(0), _local_pathname(""),
    _timestamp(true), _sndbuf(-1), _rcvbuf(-1),
    _snaplen(2048), _headroom(Packet::default_headroom), _nodelay(1),
    _verbose(false), _client(false), _proper(false), _allow(0), _deny(0)
{
}

Socket::~Socket()
{
}

int
Socket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  _client = (noutputs() == 0);
  Args args = Args(this, errh).bind(conf);
  if (args.read_mp("TYPE", socktype).execute() < 0)
    return -1;
  socktype = socktype.upper();

  // remove keyword arguments
  Element *allow = 0, *deny = 0;
  if (args.read("VERBOSE", _verbose)
      .read("SNAPLEN", _snaplen)
      .read("HEADROOM", _headroom)
      .read("TIMESTAMP", _timestamp)
      .read("RCVBUF", _rcvbuf)
      .read("SNDBUF", _sndbuf)
      .read("NODELAY", _nodelay)
      .read("CLIENT", _client)
      .read("PROPER", _proper)
      .read("ALLOW", allow)
      .read("DENY", deny)
      .consume() < 0)
    return -1;

  if (allow && !(_allow = (IPRouteTable *)allow->cast("IPRouteTable")))
    return errh->error("%s is not an IPRouteTable", allow->name().c_str());

  if (deny && !(_deny = (IPRouteTable *)deny->cast("IPRouteTable")))
    return errh->error("%s is not an IPRouteTable", deny->name().c_str());

  if (socktype == "TCP" || socktype == "UDP") {
    _family = AF_INET;
    _socktype = socktype == "TCP" ? SOCK_STREAM : SOCK_DGRAM;
    _protocol = socktype == "TCP" ? IPPROTO_TCP : IPPROTO_UDP;
    if (args.read_mp("ADDR", _remote_ip)
	.read_mp("PORT", IPPortArg(_protocol), _remote_port)
	.read_p("LOCAL_ADDR", _local_ip)
	.read_p("LOCAL_PORT", IPPortArg(_protocol), _local_port)
	.complete() < 0)
      return -1;
  }

  else if (socktype == "UNIX" || socktype == "UNIX_DGRAM") {
    _family = AF_UNIX;
    _socktype = socktype == "UNIX" ? SOCK_STREAM : SOCK_DGRAM;
    _protocol = 0;
    if (args.read_mp("FILENAME", FilenameArg(), _remote_pathname)
	.read_p("LOCAL_FILENAME", FilenameArg(), _local_pathname)
	.complete() < 0)
      return -1;
    int max_path = (int)sizeof(((struct sockaddr_un *)0)->sun_path);
    // if not in the abstract namespace (begins with zero byte),
    // reserve room for trailing NUL
    if ((_remote_pathname[0] && _remote_pathname.length() >= max_path) ||
	(_remote_pathname[0] == 0 && _remote_pathname.length() > max_path))
      return errh->error("remote filename '%s' too long", _remote_pathname.printable().c_str());
    if ((_local_pathname[0] && _local_pathname.length() >= max_path) ||
	(_local_pathname[0] == 0 && _local_pathname.length() > max_path))
      return errh->error("local filename '%s' too long", _local_pathname.printable().c_str());
  }

  else
    return errh->error("unknown socket type `%s'", socktype.c_str());

  return 0;
}


int
Socket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_fd >= 0) {
    remove_select(_fd, SELECT_READ | SELECT_WRITE);
    close(_fd);
    _fd = -1;
  }

  return errh->error("%s: %s", syscall, strerror(e));
}

int
Socket::initialize(ErrorHandler *errh)
{
  // open socket, set options
  _fd = socket(_family, _socktype, _protocol);
  if (_fd < 0)
    return initialize_socket_error(errh, "socket");

  if (_family == AF_INET) {
    _remote.in.sin_family = _family;
    _remote.in.sin_port = htons(_remote_port);
    _remote.in.sin_addr = _remote_ip.in_addr();
    _remote_len = sizeof(_remote.in);
    _local.in.sin_family = _family;
    _local.in.sin_port = htons(_local_port);
    _local.in.sin_addr = _local_ip.in_addr();
    _local_len = sizeof(_local.in);
  }
  else {
    _remote.un.sun_family = _family;
    _remote_len = offsetof(struct sockaddr_un, sun_path) + _remote_pathname.length();
    if (_remote_pathname[0]) {
      strcpy(_remote.un.sun_path, _remote_pathname.c_str());
      _remote_len++;
    } else
      memcpy(_remote.un.sun_path, _remote_pathname.c_str(), _remote_pathname.length());
    _local.un.sun_family = _family;
    _local_len = offsetof(struct sockaddr_un, sun_path) + _local_pathname.length();
    if (_local_pathname[0]) {
      strcpy(_local.un.sun_path, _local_pathname.c_str());
      _local_len++;
    } else
      memcpy(_local.un.sun_path, _local_pathname.c_str(), _local_pathname.length());
  }

  // enable timestamps
  if (_timestamp) {
#ifdef SO_TIMESTAMP
    int one = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_TIMESTAMP, &one, sizeof(one)) < 0)
      return initialize_socket_error(errh, "setsockopt(SO_TIMESTAMP)");
#else
    return initialize_socket_error(errh, "TIMESTAMP not supported on this platform");
#endif
  }

#ifdef TCP_NODELAY
  // disable Nagle algorithm
  if (_protocol == IPPROTO_TCP && _nodelay)
    if (setsockopt(_fd, IP_PROTO_TCP, TCP_NODELAY, &_nodelay, sizeof(_nodelay)) < 0)
      return initialize_socket_error(errh, "setsockopt(TCP_NODELAY)");
#endif

  // set socket send buffer size
  if (_sndbuf >= 0)
    if (setsockopt(_fd, SOL_SOCKET, SO_SNDBUF, &_sndbuf, sizeof(_sndbuf)) < 0)
      return initialize_socket_error(errh, "setsockopt(SO_SNDBUF)");

  // set socket receive buffer size
  if (_rcvbuf >= 0)
    if (setsockopt(_fd, SOL_SOCKET, SO_RCVBUF, &_rcvbuf, sizeof(_rcvbuf)) < 0)
      return initialize_socket_error(errh, "setsockopt(SO_RCVBUF)");

  // if a server, then the first arguments should be interpreted as
  // the address/port/file to bind() to, not to connect() to
  if (!_client) {
    memcpy(&_local, &_remote, _remote_len);
    _local_len = _remote_len;
  }

  // if a server, or if the optional local arguments have been
  // specified, bind() to the specified address/port/file
  if (!_client || _local_port != 0 || _local_pathname != "") {
#ifdef HAVE_PROPER
    int ret = -1;
    if (_proper) {
      ret = prop_bind_socket(_fd, (struct sockaddr *)&_local, _local_len);
      if (ret < 0)
	errh->warning("prop_bind_socket: %s", strerror(errno));
    }
    if (ret < 0)
#endif
    if (bind(_fd, (struct sockaddr *)&_local, _local_len) < 0)
      return initialize_socket_error(errh, "bind");
  }

  if (_client) {
    // connect
    if (_socktype == SOCK_STREAM) {
      if (connect(_fd, (struct sockaddr *)&_remote, _remote_len) < 0)
	return initialize_socket_error(errh, "connect");
      if (_verbose)
	click_chatter("%s: opened connection %d to %s:%d", declaration().c_str(), _fd, IPAddress(_remote.in.sin_addr).unparse().c_str(), ntohs(_remote.in.sin_port));
    }
    _active = _fd;
  } else {
    // start listening
    if (_socktype == SOCK_STREAM) {
      if (listen(_fd, 2) < 0)
	return initialize_socket_error(errh, "listen");
      if (_verbose) {
	if (_family == AF_INET)
	  click_chatter("%s: listening for connections on %s:%d (%d)", declaration().c_str(), IPAddress(_local.in.sin_addr).unparse().c_str(), ntohs(_local.in.sin_port), _fd);
	else
	  click_chatter("%s: listening for connections on %s (%d)", declaration().c_str(), _local.un.sun_path, _fd);
      }
    } else {
      _active = _fd;
    }
  }

  // nonblocking I/O and close-on-exec for the socket
  fcntl(_fd, F_SETFL, O_NONBLOCK);
  fcntl(_fd, F_SETFD, FD_CLOEXEC);

  if (noutputs())
    add_select(_fd, SELECT_READ);

  if (ninputs() && input_is_pull(0)) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    add_select(_fd, SELECT_WRITE);
  }

  return 0;
}

void
Socket::cleanup(CleanupStage)
{
  if (_active >= 0 && _active != _fd) {
    close(_active);
    _active = -1;
  }
  if (_rq)
    _rq->kill();
  if (_wq)
    _wq->kill();
  if (_fd >= 0) {
    // shut down the listening socket in case we forked
#ifdef SHUT_RDWR
    shutdown(_fd, SHUT_RDWR);
#else
    shutdown(_fd, 2);
#endif
    close(_fd);
    if (_family == AF_UNIX)
      unlink(_local_pathname.c_str());
    _fd = -1;
  }
}

bool
Socket::allowed(IPAddress addr)
{
  IPAddress gw;

  if (_allow && _allow->lookup_route(addr, gw) >= 0)
    return true;
  else if (_deny && _deny->lookup_route(addr, gw) >= 0)
    return false;
  else
    return true;
}

void
Socket::close_active(void)
{
  if (_active >= 0) {
    remove_select(_active, SELECT_READ | SELECT_WRITE);
    close(_active);
    if (_verbose)
      click_chatter("%s: closed connection %d", declaration().c_str(), _active);
    _active = -1;
  }
}

void
Socket::selected(int fd, int)
{
  int len;
  union { struct sockaddr_in in; struct sockaddr_un un; } from;
  socklen_t from_len = sizeof(from);
  bool allow;

  if (noutputs()) {
    // accept new connections
    if (_socktype == SOCK_STREAM && !_client && _active < 0 && fd == _fd) {
      _active = accept(_fd, (struct sockaddr *)&from, &from_len);

      if (_active < 0) {
	if (errno != EAGAIN)
	  click_chatter("%s: accept: %s", declaration().c_str(), strerror(errno));
	return;
      }

      if (_family == AF_INET) {
	allow = allowed(IPAddress(from.in.sin_addr));

	if (_verbose)
	  click_chatter("%s: %s connection %d from %s:%d", declaration().c_str(),
			allow ? "opened" : "denied",
			_active, IPAddress(from.in.sin_addr).unparse().c_str(), ntohs(from.in.sin_port));

	if (!allow) {
	  close(_active);
	  _active = -1;
	  return;
	}
      } else {
	if (_verbose)
	  click_chatter("%s: opened connection %d from %s", declaration().c_str(), _active, from.un.sun_path);
      }

      fcntl(_active, F_SETFL, O_NONBLOCK);
      fcntl(_active, F_SETFD, FD_CLOEXEC);

      add_select(_active, SELECT_READ | SELECT_WRITE);
    }

    // read data from socket
    if (!_rq)
      _rq = Packet::make(_headroom, 0, _snaplen, 0);
    if (_rq) {
      if (_socktype == SOCK_STREAM)
	len = read(_active, _rq->data(), _rq->length());
      else if (_client)
	len = recv(_active, _rq->data(), _rq->length(), MSG_TRUNC);
      else {
	// datagram server, find out who we are talking to
	len = recvfrom(_active, _rq->data(), _rq->length(), MSG_TRUNC, (struct sockaddr *)&from, &from_len);

	if (_family == AF_INET && !allowed(IPAddress(from.in.sin_addr))) {
	  if (_verbose)
	    click_chatter("%s: dropped datagram from %s:%d", declaration().c_str(),
			  IPAddress(from.in.sin_addr).unparse().c_str(), ntohs(from.in.sin_port));
	  len = -1;
	  errno = EAGAIN;
	} else if (len > 0) {
	  memcpy(&_remote, &from, from_len);
	  _remote_len = from_len;
	}
      }

      // this segment OK
      if (len > 0) {
	if (len > _snaplen) {
	  // truncate packet to max length (should never happen)
	  assert(_rq->length() == (uint32_t)_snaplen);
	  SET_EXTRA_LENGTH_ANNO(_rq, len - _snaplen);
	} else {
	  // trim packet to actual length
	  _rq->take(_snaplen - len);
	}

	// set timestamp
	if (_timestamp)
	  _rq->timestamp_anno().assign_now();

	// push packet
	output(0).push(_rq);
	_rq = 0;
      }

      // connection terminated or fatal error
      else if (len == 0 || errno != EAGAIN) {
	if (errno != EAGAIN && _verbose)
	  click_chatter("%s: %s", declaration().c_str(), strerror(errno));
	close_active();
	return;
      }
    }
  }

  if (ninputs() && input_is_pull(0))
    run_task(0);
}

int
Socket::write_packet(Packet *p)
{
  int len;

  assert(_active >= 0);

  while (p->length()) {
    if (!IPAddress(_remote_ip) && _client && _family == AF_INET && _socktype != SOCK_STREAM) {
      // If the IP address specified when the element was created is 0.0.0.0,
      // send the packet to its IP destination annotation address
      _remote.in.sin_addr = p->dst_ip_anno();
    }

    // write segment
    if (_socktype == SOCK_STREAM)
      len = write(_active, p->data(), p->length());
    else
      len = sendto(_active, p->data(), p->length(), 0,
		   (struct sockaddr *)&_remote, _remote_len);

    // error
    if (len < 0) {
      // out of memory or would block
      if (errno == ENOBUFS || errno == EAGAIN)
	return -1;

      // interrupted by signal, try again immediately
      else if (errno == EINTR)
	continue;

      // connection probably terminated or other fatal error
      else {
	if (_verbose)
	  click_chatter("%s: %s", declaration().c_str(), strerror(errno));
	close_active();
	break;
      }
    } else
      // this segment OK
      p->pull(len);
  }

  p->kill();
  return 0;
}

void
Socket::push(int, Packet *p)
{
  fd_set fds;
  int err;

  if (_active >= 0) {
    // block
    do {
      FD_ZERO(&fds);
      FD_SET(_active, &fds);
      err = select(_active + 1, NULL, &fds, NULL, NULL);
    } while (err < 0 && errno == EINTR);

    if (err >= 0) {
      // write
      do {
	err = write_packet(p);
      } while (err < 0 && (errno == ENOBUFS || errno == EAGAIN));
    }

    if (err < 0) {
      if (_verbose)
	click_chatter("%s: %s, dropping packet", declaration().c_str(), strerror(err));
      p->kill();
    }
  } else
    p->kill();
}

bool
Socket::run_task(Task *)
{
  assert(ninputs() && input_is_pull(0));
  bool any = false;

  if (_active >= 0) {
    Packet *p = 0;
    int err = 0;

    // write as much as we can
    do {
      p = _wq ? _wq : input(0).pull();
      _wq = 0;
      if (p) {
	any = true;
	err = write_packet(p);
      }
    } while (p && err >= 0);

    if (err < 0) {
      // queue packet for writing when socket becomes available
      _wq = p;
      p = 0;
      add_select(_active, SELECT_WRITE);
    } else if (_signal)
      // more pending
      // (can't use fast_reschedule() cause selected() calls this)
      _task.reschedule();
    else
      // wrote all we could and no more pending
      remove_select(_active, SELECT_WRITE);
  }

  // true if we wrote at least one packet
  return any;
}

void
Socket::add_handlers()
{
  add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel IPRouteTable)
EXPORT_ELEMENT(Socket)
