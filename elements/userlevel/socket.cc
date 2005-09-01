// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * socket.{cc,hh} -- transports packets via sockets
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
#include "socket.hh"

CLICK_DECLS

Socket::Socket()
  : Element(1, 1), _task(this), _timer(this),
    _fd(-1), _active(-1), _rq(0), _wq(0),
    _snaplen(2048), _nodelay(1),
    _verbose(false), _client(false)
{
}

Socket::~Socket()
{
}

void
Socket::notify_ninputs(int n)
{
  // if no inputs, then assume that this is a server socket
  if (!n)
    _client = false;
  set_ninputs(n <= 1 ? n : 1);
}

void
Socket::notify_noutputs(int n)
{
  // if no outputs, then assume that this is a client socket
  if (!n)
    _client = true;
  set_noutputs(n <= 1 ? n : 1);
}

int
Socket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket (`TCP' or `UDP' or `UNIX' or `UNIX_DGRAM')", &socktype,
		  cpIgnoreRest,
		  cpEnd) < 0)
    return -1;
  socktype = socktype.upper();

  // remove keyword arguments
  if (cp_va_parse_remove_keywords(conf, 2, this, errh,
		"VERBOSE", cpBool, "be verbose?", &_verbose,
		"SNAPLEN", cpUnsigned, "maximum packet length", &_snaplen,
		"NODELAY", cpUnsigned, "disable Nagle algorithm?", &_nodelay,
		"CLIENT", cpBool, "client or server?", &_client,
		cpEnd) < 0)
    return -1;

  if (socktype == "TCP" || socktype == "UDP") {
    _family = AF_INET;
    _socktype = socktype == "TCP" ? SOCK_STREAM : SOCK_DGRAM;
    _protocol = socktype == "TCP" ? IPPROTO_TCP : IPPROTO_UDP;
    if (cp_va_parse(conf, this, errh,
		    cpIgnore,
		    cpIPAddress, "IP address", &_ip,
		    cpUnsignedShort, "port number", &_port,
		    cpEnd) < 0)
      return -1;
  }

  else if (socktype == "UNIX" || socktype == "UNIX_DGRAM") {
    _family = AF_UNIX;
    _socktype = socktype == "UNIX" ? SOCK_STREAM : SOCK_DGRAM;
    _protocol = 0;
    if (cp_va_parse(conf, this, errh,
		    cpIgnore, cpString, "filename", &_pathname,
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
    _sa.in.sin_family = _family;
    _sa.in.sin_port = htons(_port);
    _sa.in.sin_addr = _ip.in_addr();
    _sa_len = sizeof(_sa.in);
  }
  else {
    _sa.un.sun_family = _family;
    strcpy(_sa.un.sun_path, _pathname.cc());
    _sa_len = offsetof(struct sockaddr_un, sun_path) + _pathname.length() + 1;
  }

#ifdef TCP_NODELAY
  // disable Nagle algorithm
  if (_protocol == IPPROTO_TCP && _nodelay)
    if (setsockopt(_fd, IP_PROTO_TCP, TCP_NODELAY, &_nodelay, sizeof(_nodelay)) < 0)
      return initialize_socket_error(errh, "setsockopt");
#endif

  if (_client) {
    // connect
    if (_socktype == SOCK_STREAM) {
      if (connect(_fd, (struct sockaddr *)&_sa, _sa_len) < 0)
	return initialize_socket_error(errh, "connect");
      if (_verbose)
	click_chatter("%s: opened connection %d to %s:%d", declaration().cc(), _fd, IPAddress(_sa.in.sin_addr).unparse().cc(), ntohs(_sa.in.sin_port));
    }
    _active = _fd;
  } else {
    // bind to port
    if (bind(_fd, (struct sockaddr *)&_sa, _sa_len) < 0)
      return initialize_socket_error(errh, "bind");
    // start listening
    if (_socktype == SOCK_STREAM) {
      if (listen(_fd, 2) < 0)
	return initialize_socket_error(errh, "listen");
      if (_verbose) {
	if (_family == AF_INET)
	  click_chatter("%s: listening for connections on %s:%d (%d)", declaration().cc(), IPAddress(_sa.in.sin_addr).unparse().cc(), ntohs(_sa.in.sin_port), _fd);
	else
	  click_chatter("%s: listening for connections on %s (%d)", declaration().cc(), _sa.un.sun_path, _fd);
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

  if (ninputs()) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
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
      unlink(_pathname.c_str());
    _fd = -1;
  }
}

void
Socket::selected(int fd)
{
  int len;

  if (noutputs()) {
    // accept new connections
    if (_socktype == SOCK_STREAM && !_client && _active < 0 && fd == _fd) {
      _sa_len = sizeof(_sa);
      _active = accept(_fd, (struct sockaddr *)&_sa, &_sa_len);

      if (_active < 0) {
	if (errno != EAGAIN)
	  click_chatter("%s: accept: %s", declaration().cc(), strerror(errno));
	return;
      }

      if (_verbose) {
	if (_family == AF_INET)
	  click_chatter("%s: opened connection %d from %s:%d", declaration().cc(), _active, IPAddress(_sa.in.sin_addr).unparse().cc(), ntohs(_sa.in.sin_port));
	else
	  click_chatter("%s: opened connection %d from %s", declaration().cc(), _active, _sa.un.sun_path);
      }

      fcntl(_active, F_SETFL, O_NONBLOCK);
      fcntl(_active, F_SETFD, FD_CLOEXEC);

      add_select(_active, SELECT_READ);
      _events = SELECT_READ;
    }

    // read data from socket
    if (!_rq)
      _rq = Packet::make(_snaplen);
    if (_rq) {
      if (_socktype == SOCK_STREAM)
	len = read(_active, _rq->data(), _rq->length());
      else if (_client)
	len = recv(_active, _rq->data(), _rq->length(), MSG_TRUNC);
      else {
	// datagram server, find out who we are talking to
	_sa_len = sizeof(_sa);
	len = recvfrom(_active, _rq->data(), _rq->length(), MSG_TRUNC, (struct sockaddr *)&_sa, &_sa_len);
      }
      if (len > 0) {
	if (len > _snaplen) {
	  assert(_rq->length() == (uint32_t)_snaplen);
	  SET_EXTRA_LENGTH_ANNO(_rq, len - _snaplen);
	} else
	  _rq->take(_snaplen - len);
	// set timestamp
	_rq->timestamp_anno().set_now();
	output(0).push(_rq);
	_rq = 0;
      } else {
	if (len == 0 || errno != EAGAIN) {
	  if (errno != EAGAIN)
	    click_chatter("%s: %s", declaration().cc(), strerror(errno));
	  goto err;
	}
      }
    }
  }

  if (ninputs()) {
    // write data to socket
    Packet *p;
    if (_wq) {
      p = _wq;
      _wq = 0;
    } else {
      p = input(0).pull();
    }
    if (p) {
      while (p->length()) {
	if (!IPAddress(_ip) && _client && _family == AF_INET && _socktype != SOCK_STREAM) {
	  // If the IP address specified when the element was created is 0.0.0.0, 
	  // send the packet to its IP destination annotation address
	  _sa.in.sin_addr = p->dst_ip_anno();
	}
	if (_socktype == SOCK_STREAM)
	  len = write(_active, p->data(), p->length());
	else {
	  if (_family == AF_INET)
	    _sa_len = sizeof(_sa.in);
	  else
	    _sa_len = offsetof(struct sockaddr_un, sun_path) + strlen(_sa.un.sun_path) + 1;
	  len = sendto(_active, p->data(), p->length(), 0,
		       (struct sockaddr *)&_sa, _sa_len);
	}
	if (len < 0) {
	  if (errno == ENOBUFS || errno == EAGAIN) {
	    // socket queue full, try again later
	    _wq = p;
	    remove_select(_active, SELECT_WRITE);
	    _events &= ~SELECT_WRITE;
	    _backoff = (!_backoff) ? 1 : _backoff*2;
	    _timer.schedule_after(Timestamp::make_usec(_backoff));
	    return;
	  } else if (errno == EINTR) {
	    // interrupted by signal, try again immediately
	    continue;
	  } else {
	    // connection probably terminated
	    if (_verbose)
	      click_chatter("%s: %s", declaration().cc(), _sa.un.sun_path);
	    p->kill();
	    goto err;
	  }
	} else {
	  p->pull(len);
	}
      }
      _backoff = 0;
      p->kill();
    }

    // nothing to write, wait for upstream signal
    if (!p && !_signal && (_events & SELECT_WRITE)) {
      remove_select(_active, SELECT_WRITE);
      _events &= ~SELECT_WRITE;
    }
  }

  return;

 err:
  if (_active != _fd) {
    remove_select(_active, SELECT_READ | SELECT_WRITE);
    close(_active);
    if (_verbose)
      click_chatter("%s: closed connection %d", declaration().cc(), _active);
    _active = -1;
  }
}

void
Socket::run_timer()
{
  if ((_wq || _signal) && !(_events & SELECT_WRITE) && _active >= 0) {
    add_select(_active, SELECT_WRITE);
    _events |= SELECT_WRITE;
    selected(_active);
  }
}

bool
Socket::run_task()
{
  if ((_wq || _signal) && !(_events & SELECT_WRITE) && _active >= 0) {
    add_select(_active, SELECT_WRITE);
    _events |= SELECT_WRITE;
    selected(_active);
    return true;
  }
  return false;
}

void
Socket::add_handlers()
{
  add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(Socket)
