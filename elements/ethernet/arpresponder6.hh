#ifndef ARPRESPONDER6_HH
#define ARPRESPONDER6_HH

/*
 * =c
 * ARPResponder6(IP1 MASK1 ETH1, IP2 MASK2 ETH2, ...)
 * =d
 * Input should be ARP request packets, including the
 * ethernet header.
 * Forwards an ARP reply if we know the answer.
 * Could be used for proxy ARP as well as producing
 * replies for a host's own address.
 *
 * =e
 * Produce ARP replies for the local machine (18.26.4.24)
 * as well as proxy ARP for all machines on net 18.26.7
 * directing their packets to the local machine:
 *
 *   c :: Classifier(12/0806 20/0002, ...);
 *   ar :: ARPResponder(18.26.4.24 255.255.255.255 00:00:C0:AE:67:EF,
 *                      18.26.7.0  255.255.255.0 00:00:C0:AE:67:EF)
 *   c[0] -> ar;
 *   ar -> ToDevice(eth0);
 *
 * =a
 * ARPQuerier6, ARPFaker6
 */

#include "element.hh"
#include "etheraddress.hh"
#include "ip6address.hh"
#include "vector.hh"

class ARPResponder6 : public Element {
public:
  ARPResponder6();
  ~ARPResponder6();
  
  const char *class_name() const		{ return "ARPResponder6"; }
  const char *processing() const		{ return AGNOSTIC; }
  ARPResponder6 *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
  //void set_map(IP6Address dst, IP6Address mask, EtherAddress);

  Packet *make_response(unsigned char tha[6], unsigned char tpa[16],
                        unsigned char sha[6], unsigned char spa[16]);

  bool lookup(IP6Address, EtherAddress &);

private:

  struct Entry {
    IP6Address _dst;
    IP6Address _mask;
    EtherAddress _ena;
  };
  Vector<Entry> _v;
  
  void add_map(IP6Address dst, IP6Address mask, EtherAddress);

};

#endif
