#ifndef ARPRESPONDER_HH
#define ARPRESPONDER_HH

/*
 * =c
 * ARPResponder(IP1 MASK1 ETH1, IP2 MASK2 ETH2, ...)
 * ARPResponder(IP1 [MASK1], IP2 [MASK2], ..., ETH)
 * =d
 * Input should be ARP request packets, including the
 * Ethernet header.
 * Forwards an ARP reply if we know the answer.
 * Could be used for proxy ARP as well as producing
 * replies for a host's own address.
 *
 * The IP address and netmask can be specified in dotted decimal form (for
 * example, `<tt>18.26.7.0 255.255.255.0</tt>') or CIDR form (for example,
 * `<tt>18.26.7.0/24</tt>').
 *
 * =e
 * Produce ARP replies for the local machine (18.26.4.24)
 * as well as proxy ARP for all machines on net 18.26.7
 * directing their packets to the local machine:
 *
 * = c :: Classifier(12/0806 20/0002, ...);
 * = ar :: ARPResponder(18.26.4.24, 18.26.7.0/24, 00:00:C0:AE:67:EF);
 * = c[0] -> ar;
 * = ar -> ToDevice(eth0);
 *
 * =a ARPQuerier
 * =a ARPFaker */

#include "element.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "vector.hh"

class ARPResponder : public Element {

 public:
  ARPResponder();
  ~ARPResponder();
  
  const char *class_name() const		{ return "ARPResponder"; }
  const char *processing() const	{ return AGNOSTIC; }
  ARPResponder *clone() const;
  int configure(const String &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
  Packet *make_response(unsigned char tha[6], unsigned char tpa[4],
                        unsigned char sha[6], unsigned char spa[4]);

  bool lookup(IPAddress, EtherAddress &);

private:

  struct Entry {
    IPAddress _dst;
    IPAddress _mask;
    EtherAddress _ena;
  };
  Vector<Entry> _v;
  
  void add_map(IPAddress dst, IPAddress mask, EtherAddress);
  
};

#endif
