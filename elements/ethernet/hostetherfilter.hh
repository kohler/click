#ifndef CLICK_HOSTETHERFILTER_HH
#define CLICK_HOSTETHERFILTER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
=c

HostEtherFilter(ETHER [, DROP_OWN, DROP_OTHER, I<KEYWORDS>])

=s ethernet

drops Ethernet packets sent to other machines

=d

Expects Ethernet packets as input. Acts basically like Ethernet input hardware
for a device with address ETHER.

In particular, HostEtherFilter sets each packet's packet type annotation to
HOST, BROADCAST, MULTICAST, or OTHERHOST based on its Ethernet destination
address. Emits most packets on the first output. If DROP_OWN is true, drops
packets whose source address is ETHER; defaults to false. If DROP_OTHER is
true, drops packets sent to hosts other than ETHER (that is, packets with
unicast destination addresses not equal to ETHER); defaults to true. If the
element has two outputs, filtered packets are emitted on the second output
rather than dropped.

Keyword arguments are:

=over 8

=item DROP_OWN

Same as the DROP_OWN parameter.

=item DROP_OTHER

Same as the DROP_OTHER parameter.

=item OFFSET

The ethernet header starts OFFSET bytes into the packet. Default OFFSET is 0.

=back

*/

class HostEtherFilter : public Element { public:

  HostEtherFilter() CLICK_COLD;
  ~HostEtherFilter() CLICK_COLD;

  const char *class_name() const		{ return "HostEtherFilter"; }
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PROCESSING_A_AH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);
  void add_handlers() CLICK_COLD;

 private:

  bool _drop_own : 1;
  bool _drop_other : 1;
  int _offset;
  EtherAddress _addr;

};

CLICK_ENDDECLS
#endif
