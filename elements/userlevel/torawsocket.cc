// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * torawsocket.{cc,hh} -- element write data to socket
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
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "fromrawsocket.hh"
#include "torawsocket.hh"

CLICK_DECLS

ToRawSocket::ToRawSocket()
  : Element(1, 0), _task(this), _fd(-1)
{
}

ToRawSocket::~ToRawSocket()
{
}

void
ToRawSocket::notify_noutputs(int n)
{
  set_noutputs(n <= 1 ? n : 1);
}

int
ToRawSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String socktype;
  if (cp_va_parse(conf, this, errh,
		  cpString, "type of socket (`TCP', `UDP', `GRE', `ICMP')", &socktype,
		  cpUnsignedShort, "port number", &_port,
		  cpIgnoreRest,
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
    return errh->error("unknown socket type `%s'", socktype.cc());

  return 0;
}


int
ToRawSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_my_fd && _fd >= 0)
    close(_fd);
  _fd = -1;

  return errh->error("%s: %s", syscall, strerror(e));
}

int
ToRawSocket::initialize(ErrorHandler *errh)
{
  struct sockaddr_in sin;

  // find a FromRawSocket and reuse its socket if possible
  click_chatter("%s: looking for a FromRawSocket(%d, %d)\n", declaration().cc(), _protocol, _port);
  for (int ei = 0; ei < router()->nelements() && _fd < 0; ei++) {
    Element *e = router()->element(ei);
    FromRawSocket *frs = (FromRawSocket *)e->cast("FromRawSocket");
    if (frs && frs->protocol() == _protocol && frs->port() == _port) {
      click_chatter("%s: found a FromRawSocket(%d, %d)\n", declaration().cc(), _protocol, _port);
      _fd = frs->fd();
      _my_fd = false;
    }
  }

  // open socket, set options, bind to address
  if (_fd < 0) {
    _fd = socket(PF_INET, SOCK_RAW, _protocol);
    if (_fd < 0)
      return initialize_socket_error(errh, "socket");
    _my_fd = true;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    sin.sin_port = htons(_port);
    sin.sin_addr = inet_makeaddr(0, 0);

    // bind to port
    if (bind(_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
      return initialize_socket_error(errh, "bind");
  }

  // include IP header
  int one = 1;
  if (setsockopt(_fd, 0, IP_HDRINCL, &one, sizeof(one)) < 0)
    return initialize_socket_error(errh, "IP_HDRINCL");

  if (input_is_pull(0)) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
  }
  return 0;
}

void
ToRawSocket::cleanup(CleanupStage)
{
  if (_my_fd && _fd >= 0)
    close(_fd);
  _fd = -1;
}

void
ToRawSocket::send_packet(Packet *p)
{
  int w = 0;
  const click_ip *ip = (const click_ip *) p->data();
  const click_udp *udp = (const click_udp *) &ip[1];
  const click_icmp_sequenced *icmp = (const click_icmp_sequenced *) &ip[1];
  // const click_gre *gre = (const click_gre *) &ip[1];
  // const click_pptp *pptp = (const click_pptp *) &ip[1];
  struct sockaddr_in sin;

  // cast to int so very large plen is interpreted as negative 
  if ((int)p->length() < (int)sizeof(click_ip)) {
    click_chatter("%s: runt IP packet (%d bytes)", declaration().cc(), p->length());
    goto done;
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = PF_INET;
  sin.sin_addr = ip->ip_dst;

  switch (ip->ip_p) {
  case IPPROTO_TCP:
  case IPPROTO_UDP:
    if ((int)p->length() < (int)sizeof(click_ip) + (int)sizeof(click_udp)) {
      click_chatter("%s: runt %s packet (%d bytes)", declaration().cc(),
		    ip->ip_p == IPPROTO_TCP ? "TCP" : "UDP", p->length());
      goto done;
    }
    sin.sin_port = udp->uh_dport;
    break;
  case IPPROTO_ICMP:
    if ((int)p->length() < (int)sizeof(click_ip) + (int)sizeof(click_icmp_sequenced)) {
      click_chatter("%s: runt ICMP packet (%d bytes)", declaration().cc(), p->length());
      goto done;
    }
    sin.sin_port = icmp->icmp_identifier;
    break;
  case IPPROTO_GRE:
    // XXX handle GRE keys/PPTP call IDs
  default:
    click_chatter("%s: unhandled raw IP protocol %d", declaration().cc(), ip->ip_p);
    goto done;
  }

  while (p->length()) {
    w = sendto(_fd, p->data(), p->length(), 0, (const struct sockaddr*)&sin, sizeof(sin));
    if (w < 0 && errno != EINTR)
      break;
    p->pull(w);
  }
  if (w < 0) {
    click_chatter("%s: sendto: %s", declaration().cc(), strerror(errno));
    checked_output_push(0, p);
    return;
  }
 done:
    p->kill();
}

void
ToRawSocket::push(int, Packet *p)
{
  send_packet(p);
}

bool
ToRawSocket::run_task()
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
ToRawSocket::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(ToRawSocket)
