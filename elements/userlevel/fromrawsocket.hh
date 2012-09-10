// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_FROMRAWSOCKET_HH
#define CLICK_FROMRAWSOCKET_HH
#include <click/element.hh>
#include "rawsocket.hh"
CLICK_DECLS

/*
=c

FromRawSocket("TCP", <TCP source port number> [, I<KEYWORDS>])
FromRawSocket("UDP", <UDP source port number> [, I<KEYWORDS>])
FromRawSocket("GRE", <GRE key or PPTP call ID> [, I<KEYWORDS>])
FromRawSocket("ICMP", <ICMP identifier> [, I<KEYWORDS>])

=s comm

reads raw IP packets from safe raw socket (user-level)

=d

Reads data from a raw IPv4 socket. The raw IPv4 socket may optionally
be bound to a source port number in the case of TCP/UDP, a GRE key or
PPTP call ID in the case of GRE, or an identifier in the case of
ICMP. Binding a port to a raw IPv4 socket to reserve it and suppress
TCP RST and ICMP Unreachable errors, is specific to PlanetLab Linux.

This element exists only for backward compatibility. See the more
general RawSocket implementation for details, and for supported
keyword arguments. A FromRawSocket is equivalent to a RawSocket with
no inputs.

=e

  FromRawSocket(UDP, 53) -> ...

=a ToRawSocket, RawSocket, Socket */

class FromRawSocket : public RawSocket { public:

  FromRawSocket() CLICK_COLD;
  ~FromRawSocket() CLICK_COLD;

  const char *class_name() const	{ return "FromRawSocket"; }
  const char *processing() const	{ return PUSH; }
  const char *flow_code() const		{ return "x/y"; }

};

CLICK_ENDDECLS
#endif
