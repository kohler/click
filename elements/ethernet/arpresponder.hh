#ifndef ARPRESPONDER_HH
#define ARPRESPONDER_HH

/*
 * =c
 * ARPResponder(IP/MASK1 [IP/MASK...] ETH1, IP/MASK2 ETH2, ...)
 * =s
 * generates responses to ARP queries
 * =d
 *
 * Input should be ARP request packets, including the Ethernet header.
 * Forwards an ARP reply if we know the answer -- that is, if one of the
 * IPPREFIX arguments matches the requested IP address, then it outputs an ARP
 * reply giving the corresponding ETH address. Could be used for proxy ARP as
 * well as producing replies for a host's own address.
 *
 * The IP/MASK arguments are IP network addresses (IP address/netmask pairs).
 * The netmask can be specified in dotted decimal form
 * (`C<18.26.7.0/255.255.255.0>') or CIDR form
 * (`C<18.26.7.0/24>').
 *
 * =n
 * AddressInfo elements can simplify the arguments to ARPResponder. In
 * particular, if C<NAME> is shorthand for both an IP network address (or IP
 * address) C<IP> and an Ethernet address C<ETH>, then C<ARPResponder(NAME)>
 * is equivalent to C<ARPResponder(IP ETH)>.
 *
 * =e
 * Produce ARP replies for the local machine (18.26.4.24)
 * as well as proxy ARP for all machines on net 18.26.7
 * directing their packets to the local machine:
 *
 *   c :: Classifier(12/0806 20/0001, ...);
 *   ar :: ARPResponder(18.26.4.24 18.26.7.0/24 00:00:C0:AE:67:EF);
 *   c[0] -> ar;
 *   ar -> ToDevice(eth0);
 *
 * =a
 * ARPQuerier, ARPFaker, AddressInfo */

#include "element.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "vector.hh"

class ARPResponder : public Element {

 public:
  ARPResponder();
  ~ARPResponder();
  
  const char *class_name() const		{ return "ARPResponder"; }
  const char *processing() const		{ return AGNOSTIC; }
  ARPResponder *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

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
