#ifndef IP6NDADVERTISER_HH
#define IP6NDADVERTISER_HH

/*
 * =c
 * IP6NDAdvertiser(IP61 MASK1 ETH1, IP62 MASK2 ETH2, ...)
 * =s
 * V<ARP>
 * =d
 * Input should be Neighborhood Solitation Message (sort of ARP request 
 * packets, including the ethernet header, ip6 header and message itself.
 * Forwards an Neighborhood Advertisement Message (sort of ARP reply )
 * if we know the answer.
 * Could be used for proxy ARP as well as producing
 * replies for a host's own address.
 *
 * =e
 * Produce Neighborhood Advertisement for the local machine (3ffe:1ce1:2::5)
 * as well as proxy ARP for all machines on net 3ffe:1ce1:2::/64)
 * directing their packets to the local machine:
 *
 *   c :: Classifier(12/86dd 54/87, ...);
 *   ar :: IP6NDAdvertiser(3ffe:1ce1:2::5/128 00:00:C0:AE:67:EF,
 *               3ffe:1ce1:2::/80 00:00:C0:AE:67:EF)
 *   c[0] -> ar;
 *   ar -> ToDevice(eth0);
 *
 * =a
 * NDSol
 */

#include "element.hh"
#include "etheraddress.hh"
#include "ip6address.hh"
#include "vector.hh"

class IP6NDAdvertiser : public Element {
public:
  IP6NDAdvertiser();
  ~IP6NDAdvertiser();
  
  const char *class_name() const		{ return "IP6NDAdvertiser"; }
  const char *processing() const		{ return AGNOSTIC; }
  IP6NDAdvertiser *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
  //void set_map(IP6Address dst, IP6Address mask, EtherAddress);

  //response to multicast neighborhood solitation message 
  // which is querying the ethernet address of the targest IP6 address
  Packet *make_response(unsigned char dha[6], unsigned char sha[6],
                        unsigned char dpa[16], unsigned char spa[16],
			unsigned char tpa[16], unsigned char tha[6]);


  //response to unicast neighborhood solitation message 
  //which is veryfying the ethernet address of the targest IP6 address
  Packet *make_response2(unsigned char dha[6], unsigned char sha[6],
                        unsigned char dpa[16], unsigned char spa[16],
			unsigned char tpa[16]);

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
