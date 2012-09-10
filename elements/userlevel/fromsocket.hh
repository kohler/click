// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_FROMSOCKET_HH
#define CLICK_FROMSOCKET_HH
#include <click/element.hh>
#include "socket.hh"
CLICK_DECLS

/*
=c

FromSocket("TCP", IP, PORTNUMBER [, I<KEYWORDS>])
FromSocket("UDP", IP, PORTNUMBER [, I<KEYWORDS>])
FromSocket("UNIX", FILENAME [, I<KEYWORDS>])
FromSocket("UNIX_DGRAM", FILENAME [, I<KEYWORDS>])

=s comm

reads data from socket (user-level)

=d

Reads data from the specified socket. Packets received from the remote
host or process are emitted on the output.

This element exists only for backward compatibility. See the more
general Socket implementation for details, and for supported keyword
arguments. A FromSocket is equivalent to a Socket with the CLIENT
keyword set to FALSE or a Socket with no inputs.

=e

  FromSocket(TCP, 0.0.0.0, 80) -> ...

=a ToSocket, Socket */

class FromSocket : public Socket { public:

  FromSocket() CLICK_COLD;
  ~FromSocket() CLICK_COLD;

  const char *class_name() const	{ return "FromSocket"; }
  const char *processing() const	{ return PUSH; }
  const char *flow_code() const		{ return "x/y"; }

};

CLICK_ENDDECLS
#endif
