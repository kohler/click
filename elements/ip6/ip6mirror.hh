#ifndef CLICK_IP6MIRROR_HH
#define CLICK_IP6MIRROR_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

IP6Mirror

=s ip6

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
  const char *port_count() const		{ return PORTS_1_1; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif /* IP6MIRROR_HH */
