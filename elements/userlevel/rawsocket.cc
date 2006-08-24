// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * rawsocket.{cc,hh} -- transports raw IP packets via safe raw sockets (user-level)
 *
 * Mark Huang <mlhuang@cs.princeton.edu>
 *
 * Copyright (c) 2004-2005  The Trustees of Princeton University (Trustees).
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
#include "rawsocket.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifndef __sun
#include <sys/ioctl.h>
#else
#include <sys/ioccom.h>
#endif

#ifdef HAVE_PROPER
#include <proper/prop.h>
#endif

#include "fakepcap.hh"

CLICK_DECLS

RawSocket::RawSocket()
  : _task(this), _timer(this),
    _fd(-1), _port(0), _proper(false), _snaplen(2048),
    _headroom(Packet::DEFAULT_HEADROOM), _rq(0), _wq(0)
{
}

RawSocket::~RawSocket()
{
}

int
RawSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket (`TCP', `UDP', `GRE', `ICMP')", &socktype,
		  cpOptional,
		  cpUnsignedShort, "port number", &_port,
		  cpKeywords,
		  "SNAPLEN", cpUnsigned, "maximum packet length", &_snaplen,
		  "HEADROOM", cpUnsigned, "how much header to allocate for the packet", &_headroom,
		  "PROPER", cpBool, "use Proper", &_proper,
		  cpEnd) < 0)
    return -1;
  socktype = socktype.upper();

  if (socktype == "TCP")
    _protocol = IPPROTO_TCP;
  else if (socktype == "UDP")
    _protocol = IPPROTO_UDP;
  else if (socktype == "GRE")
    _protocol = IPPROTO_GRE;
  else if (socktype == "ICMP")
    _protocol = IPPROTO_ICMP;
  else
    return errh->error("unknown socket type `%s'", socktype.c_str());

  return 0;
}


int
RawSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_fd >= 0) {
    close(_fd);
    _fd = -1;
  }

  return errh->error("%s: %s", syscall, strerror(e));
}

int
RawSocket::initialize(ErrorHandler *errh)
{
  struct sockaddr_in sin;

  // open socket, set options, bind to address
  _fd = socket(PF_INET, SOCK_RAW, _protocol);
  if (_fd < 0)
    return initialize_socket_error(errh, "socket");

  if (_port) {
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    sin.sin_port = htons(_port);
    sin.sin_addr = inet_makeaddr(0, 0);

    // bind to port
#ifdef HAVE_PROPER
    int ret = -1;
    if (_proper) {
      ret = prop_bind_socket(_fd, (struct sockaddr *)&sin, sizeof(sin));
      if (ret < 0)
	errh->warning("prop_bind_socket: %s", strerror(errno));
    }
    if (ret < 0)
#endif
    if (bind(_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
      return initialize_socket_error(errh, "bind");
  }

  // nonblocking I/O and close-on-exec for the socket
  fcntl(_fd, F_SETFL, O_NONBLOCK);
  fcntl(_fd, F_SETFD, FD_CLOEXEC);

  // include IP header
  int one = 1;
  if (setsockopt(_fd, 0, IP_HDRINCL, &one, sizeof(one)) < 0)
    return initialize_socket_error(errh, "IP_HDRINCL");

  if (noutputs())
    add_select(_fd, SELECT_READ);

  if (ninputs()) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
  }
  return 0;
}

void
RawSocket::cleanup(CleanupStage)
{
  if (_rq)
    _rq->kill();
  if (_wq)
    _wq->kill();
  if (_fd >= 0) {
    close(_fd);
    remove_select(_fd, SELECT_READ | SELECT_WRITE);
    _fd = -1;
  }
}

void
RawSocket::selected(int fd)
{
  ErrorHandler *errh = ErrorHandler::default_handler();
  int len;

  if (noutputs()) {
    // read data from socket
    if (!_rq)
      _rq = Packet::make(_headroom, (const unsigned char *)0, _snaplen, 0);
    if (_rq) {
      len = recv(_fd, _rq->data(), _rq->length(), MSG_TRUNC);
      if (len > 0) {
	if (len > _snaplen) {
	  assert(_rq->length() == (uint32_t)_snaplen);
	  SET_EXTRA_LENGTH_ANNO(_rq, len - _snaplen);
	} else
	  _rq->take(_snaplen - len);
	// set timestamp
	(void) ioctl(fd, SIOCGSTAMP, &_rq->timestamp_anno());
	// set IP annotations
	if (fake_pcap_force_ip(_rq, FAKE_DLT_RAW))
	  output(0).push(_rq);
	else
	  _rq->kill();
	_rq = 0;
      } else {
	if (len == 0 || errno != EAGAIN) {
	  if (errno != EAGAIN)
	    errh->error("recv: %s", strerror(errno));
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
      // cast to int so very large plen is interpreted as negative 
      if ((int)p->length() < (int)sizeof(click_ip)) {
	errh->error("runt IP packet (%d bytes)", p->length());
	p->kill();
      } else {
	const click_ip *ip = (const click_ip *) p->data();
	struct sockaddr_in sin;

	// set up destination
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = PF_INET;
	sin.sin_addr = ip->ip_dst;

	while (p->length()) {
	  // send packet
	  len = sendto(_fd, p->data(), p->length(), 0,
		       (const struct sockaddr*)&sin, sizeof(sin));
	  if (len < 0) {
	    if (errno == ENOBUFS || errno == EAGAIN) {
	      // socket queue full, try again later
	      _wq = p;
	      remove_select(_fd, SELECT_WRITE);
	      _events &= ~SELECT_WRITE;
	      _backoff = (!_backoff) ? 1 : _backoff*2;
	      _timer.schedule_after(Timestamp::make_usec(_backoff));
	      return;
	    } else if (errno == EINTR) {
	      // interrupted by signal, try again immediately
	      continue;
	    } else {
	      // unexpected error: drop packet
	      errh->error("sendto: %s", strerror(errno));
	      break;
	    }
	  } else {
	    p->pull(len);
	  }
	}
	_backoff = 0;
	p->kill();
      }
    }

    // nothing to write, wait for upstream signal
    if (!p && !_signal && (_events & SELECT_WRITE)) {
      remove_select(_fd, SELECT_WRITE);
      _events &= ~SELECT_WRITE;
    }
  }
}

void
RawSocket::run_timer(Timer *)
{
  if ((_wq || _signal) && !(_events & SELECT_WRITE) && _fd >= 0) {
    add_select(_fd, SELECT_WRITE);
    _events |= SELECT_WRITE;
    selected(_fd);
  }
}

bool
RawSocket::run_task(Task *)
{
  if ((_wq || _signal) && !(_events & SELECT_WRITE) && _fd >= 0) {
    add_select(_fd, SELECT_WRITE);
    _events |= SELECT_WRITE;
    selected(_fd);
    return true;
  }
  return false;
}

void
RawSocket::add_handlers()
{
  add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(RawSocket)
