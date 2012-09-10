// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_TOSOCKET_HH
#define CLICK_TOSOCKET_HH
#include <click/element.hh>
#include "socket.hh"
CLICK_DECLS

/*
=c

ToSocket("TCP", IP, PORTNUMBER [, I<KEYWORDS>])
ToSocket("UDP", IP, PORTNUMBER [, I<KEYWORDS>])
ToSocket("UNIX", FILENAME [, I<KEYWORDS>])
ToSocket("UNIX_DGRAM", FILENAME [, I<KEYWORDS>])

=s comm

sends data to socket (user-level)

=d

Sends data to the specified socket. Input packets are sent to the
remote host or process.

This element exists only for backward compatibility. See the more
general Socket implementation for details, and for supported keyword
arguments. A ToSocket is equivalent to a Socket with the CLIENT
keyword set to TRUE or a Socket with no outputs.

=e

  ... -> ToSocket(1.2.3.4, UDP, 47)

=a FromSocket, Socket */

class ToSocket : public Socket { public:

  ToSocket() CLICK_COLD;
  ~ToSocket() CLICK_COLD;

  const char *class_name() const	{ return "ToSocket"; }
  const char *processing() const	{ return PULL; }
  const char *flow_code() const		{ return "x/y"; }

};

CLICK_ENDDECLS
#endif
