#ifndef HOSTETHERFILTER_HH
#define HOSTETHERFILTER_HH

/*
 * =c
 * HostEtherFilter(mac-addr)
 * =d
 * Expects ethernet packets as input. Discards packets that
 * aren't addressed to mac-addr or to a broadcast or
 * multicast address. That is, tries to act like
 * Ethernet input hardware.
 */

#include "element.hh"
class EtherAddress;

class HostEtherFilter : public Element {
  
 public:
  
  HostEtherFilter();
  ~HostEtherFilter();

  const char *class_name() const		{ return "HostEtherFilter"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  HostEtherFilter *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
private:

  unsigned char _addr[6];

};

#endif
