#ifndef IPMIRROR_HH
#define IPMIRROR_HH
#include <click/element.hh>

/*
=c

IPMirror

=s IP, modification

swaps IP source and destination

=d

Incoming packets must have their IP header annotations set. Swaps packets'
source and destination IP addresses. Packets containing TCP or UDP
headers---that is, first fragments of packets with protocol 6 or 17---also
have their source and destination ports swapped.

The IP and TCP or UDP checksums are not changed. They don't need to be; these
swap operations do not affect checksums.

*/

class IPMirror : public Element {

 public:

  IPMirror();
  ~IPMirror();
  
  const char *class_name() const		{ return "IPMirror"; }
  const char *processing() const		{ return AGNOSTIC; }
  IPMirror *clone() const			{ return new IPMirror; }
  
  Packet *simple_action(Packet *);
  
};

#endif IPMIRROR_HH
