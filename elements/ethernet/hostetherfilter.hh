#ifndef HOSTETHERFILTER_HH
#define HOSTETHERFILTER_HH

/*
 * =c
 * HostEtherFilter(MACADDR [, DROP-OWN])
 * =s
 * drops Ethernet packets sent to other machines
 * V<dropping>
 * =d
 *
 * Expects ethernet packets as input. Pushes packets that aren't addressed to
 * MACADDR or to a broadcast or multicast address to the second output, or
 * discards them if there is no second output. If DROP-OWN is true, drops
 * packets originated by MACADDR; defaults to false. That is, tries to act
 * like Ethernet input hardware. */

#include <click/element.hh>

class HostEtherFilter : public Element {

  bool _drop_own;
  unsigned char _addr[6];

  inline Packet *drop(Packet *);
  
 public:
  
  HostEtherFilter();
  ~HostEtherFilter();

  const char *class_name() const		{ return "HostEtherFilter"; }
  const char *processing() const		{ return "a/ah"; }

  void notify_noutputs(int);
  HostEtherFilter *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
};

#endif
