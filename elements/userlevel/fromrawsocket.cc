// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * fromrawsocket.{cc,hh} -- element reads PlanetLab 2.0 safe raw IPv4
 * socket data
 *
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
#include "fromrawsocket.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
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

#include "fakepcap.hh"

// for setsockopt()
#define SO_RAW_SNIFF     101

CLICK_DECLS

FromRawSocket::FromRawSocket()
  : Element(0, 1), _fd(-1), _snaplen(2048), _sniff(false)
{
  MOD_INC_USE_COUNT;
}

FromRawSocket::~FromRawSocket()
{
  MOD_DEC_USE_COUNT;
}

int
FromRawSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket (`TCP', `UDP', `GRE', `ICMP', `ICMP_TCP', `ICMP_UDP', `ICMP_GRE')", &socktype,
		  cpUnsignedShort, "port number", &_port,
		  cpIgnoreRest,
		  cpEnd) < 0)
    return -1;
  socktype = socktype.upper();

  // remove keyword arguments
  if (cp_va_parse_remove_keywords(conf, 1, this, errh,
		"SNAPLEN", cpUnsigned, "maximum packet length", &_snaplen,
		"SNIFF", cpBool, "sniffer socket?", &_sniff,
		cpEnd) < 0)
    return -1;

  if (socktype == "TCP")
    _protocol = IPPROTO_TCP;
  else if (socktype == "UDP")
    _protocol = IPPROTO_UDP;
  else if (socktype == "GRE")
    _protocol = IPPROTO_GRE;
  else if (socktype == "ICMP")
    _protocol = IPPROTO_ICMP;
  else if (socktype == "ICMP_TCP")
    _protocol = IPPROTO_TCP + 100;
  else if (socktype == "ICMP_UDP")
    _protocol = IPPROTO_UDP + 100;
  else if (socktype == "ICMP_GRE")
    _protocol = IPPROTO_GRE + 100;
  else
    return errh->error("unknown socket type `%s'", socktype.cc());

  return 0;
}


int
FromRawSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_fd >= 0) {
    close(_fd);
    _fd = -1;
  }

  return errh->error("%s: %s", syscall, strerror(e));
}

int
FromRawSocket::initialize(ErrorHandler *errh)
{
  struct sockaddr_in sin;

  // open socket, set options, bind to address
  _fd = socket(PF_INET, SOCK_RAW, _protocol);
  if (_fd < 0)
    return initialize_socket_error(errh, "socket");

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = PF_INET;
  sin.sin_port = htons(_port);
  sin.sin_addr = inet_makeaddr(0, 0);

  // bind to port
  if (bind(_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    return initialize_socket_error(errh, "bind");

  click_chatter("%s(%d, %d)\n", declaration().cc(), _protocol, _port);

  // create a sniffer socket
  if (_sniff) {
    int one = 1;
    if (setsockopt(_fd, 0, SO_RAW_SNIFF, &one, sizeof(one)) < 0)
      return initialize_socket_error(errh, "setsockopt");
  }

  // nonblocking I/O and close-on-exec for the socket
  fcntl(_fd, F_SETFL, O_NONBLOCK);
  fcntl(_fd, F_SETFD, FD_CLOEXEC);

  add_select(_fd, SELECT_READ);
  return 0;
}

void
FromRawSocket::cleanup(CleanupStage)
{
  if (_fd >= 0) {
    close(_fd);
    remove_select(_fd, SELECT_READ);
    _fd = -1;
  }
}

void
FromRawSocket::selected(int fd)
{
  ErrorHandler *errh = ErrorHandler::default_handler();
  int len;

  do {
    WritablePacket *p = Packet::make(_snaplen);
    if (!p)
      break;
    p->take(p->length());

    // read data from socket
    len = read(fd, p->end_data(), p->tailroom());

    if (len > 0) {
      p = p->put(len);
      if (!p)
	break;
      // set timestamp
      (void) ioctl(fd, SIOCGSTAMP, &p->timestamp_anno());
      // set IP annotations
      if (fake_pcap_force_ip(p, FAKE_DLT_RAW)) {
	output(0).push(p);
	continue;
      }
    }

    p->kill();
    if (len <= 0 && errno != EAGAIN)
      errh->error("%s: read: %s", declaration().cc(), strerror(errno));
  } while (len > 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(FromRawSocket)
