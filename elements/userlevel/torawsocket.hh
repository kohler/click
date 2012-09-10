// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_TORAWSOCKET_HH
#define CLICK_TORAWSOCKET_HH
#include <click/element.hh>
#include "rawsocket.hh"
CLICK_DECLS

/*
=c

ToRawSocket("TCP", <TCP source port number>)
ToRawSocket("UDP", <UDP source port number>)
ToRawSocket("GRE", <GRE key or PPTP call ID>)
ToRawSocket("ICMP", <ICMP identifier>)

=s comm

sends IP packets through a safe raw socket (user-level)

=d

Writes data to a raw IPv4 socket. The raw IPv4 socket may optionally
be bound to a source port number in the case of TCP/UDP, a GRE key or
PPTP call ID in the case of GRE, or an identifier in the case of
ICMP. Binding a port to a raw IPv4 socket to reserve it and suppress
TCP RST and ICMP Unreachable errors, is specific to PlanetLab Linux.

This element exists only for backward compatibility. See the more
general RawSocket implementation for details, and for supported
keyword arguments. A ToRawSocket is equivalent to a RawSocket with
no outputs.

=e

  ... -> ToRawSocket(UDP, 47)

=a FromRawSocket, RawSocket, Socket */

class ToRawSocket : public RawSocket { public:

  ToRawSocket() CLICK_COLD;
  ~ToRawSocket() CLICK_COLD;

  const char *class_name() const	{ return "ToRawSocket"; }
  const char *processing() const	{ return PULL; }
  const char *flow_code() const		{ return "x/y"; }

};

CLICK_ENDDECLS
#endif
