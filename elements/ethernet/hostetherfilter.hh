#ifndef HOSTETHERFILTER_HH
#define HOSTETHERFILTER_HH
#include <click/element.hh>

/*
=c

HostEtherFilter(ETHER [, DROP_OWN, DROP_OTHER, I<KEYWORDS>])

=s dropping, Ethernet

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

=back

*/

class HostEtherFilter : public Element { public:
  
  HostEtherFilter();
  ~HostEtherFilter();

  const char *class_name() const		{ return "HostEtherFilter"; }
  const char *processing() const		{ return "a/ah"; }

  void notify_noutputs(int);
  HostEtherFilter *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
 private:

  bool _drop_own : 1;
  bool _drop_other : 1;
  unsigned char _addr[6];

  inline Packet *drop(Packet *);
  
};

#endif
