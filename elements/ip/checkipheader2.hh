#ifndef CHECKIPHEADER2_HH
#define CHECKIPHEADER2_HH

/*
=c

CheckIPHeader2([BADADDRS, OFFSET, I<KEYWORDS>])

=s IP, checking

checks IP header, no checksum

=d

Input packets should have IP headers starting OFFSET bytes in. Default
OFFSET is zero. Checks that the packet's length is reasonable, and that the
IP version, header length, and length fields are valid. Checks that the IP
source address is a legal unicast address -- that is, that it is not
0.0.0.0 or 255.255.255.255, or one of the local broadcast addresses in
BADADDRS. Shortens packets to the IP length, if the IP length is shorter
than the nominal packet length (due to Ethernet padding, for example).
Pushes invalid packets out on output 1, unless output 1 was unused; if so,
drops invalid packets.

The BADADDRS argument is a space-separated list of IP addresses that are not
to be tolerated as source addresses. Usually consists of the subnet broadcast
addresses on each interface (RFC1812 5.3.7).

Prints a message to the console the first time it encounters an incorrect IP
packet (but see VERBOSE below).

This element does all the work of CheckIPHeader except for checking the
checksum.

Keyword arguments are:

=over 5

=item VERBOSE

Boolean. If it is true, then a message will be printed for every erroneous
packet, rather than just the first. False by default.

=item DETAILS

Boolean. If it is true, then CheckIPHeader2 will maintain detailed counts of
how many packets were dropped for each possible reason, accessible through the
C<drop_details> handler. False by default.

=back

=h drops read-only

Returns the number of incorrect packets CheckIPHeader has seen.

=h drop_details read-only

Returns a text file showing how many erroneous packets CheckIPHeader has seen,
subdivided by error. Only available if the DETAILS keyword argument was true.

=a CheckIPHeader, StripIPHeader, MarkIPHeader */

#include "elements/ip/checkipheader.hh"

class CheckIPHeader2 : public CheckIPHeader { public:
  
  CheckIPHeader2();
  ~CheckIPHeader2();
  
  const char *class_name() const		{ return "CheckIPHeader2"; }
  const char *processing() const		{ return "a/ah"; }
  CheckIPHeader2 *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

};

#endif
