#ifndef IP6MIRROR_HH
#define IP6MIRROR_HH
#include <click/element.hh>

/*
=c

IP6Mirror

=s IP6, modification

swaps IP6 source and destination

=d

Incoming packets must have their IP6 header annotations set. Swaps packets'
source and destination IP6 addresses. Packets containing TCP or UDP
headers---that is, first fragments of packets with protocol 6 or 17---also
have their source and destination ports swapped.

The TCP or UDP checksums are not changed. They don't need to be; these
swap operations do not affect checksums.

*/

class IP6Mirror : public Element {

 public:

  IP6Mirror();
  ~IP6Mirror();
  
  const char *class_name() const		{ return "IP6Mirror"; }
  const char *processing() const		{ return AGNOSTIC; }
  IP6Mirror *clone() const			{ return new IP6Mirror; }
  
  Packet *simple_action(Packet *);
  
};

#endif IP6MIRROR_HH
