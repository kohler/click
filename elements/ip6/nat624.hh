#ifndef NAT624_HH
#define NAT624_HH

#include "ip6address.hh"
#include "ipaddress.hh"
#include "vector.hh"
#include "element.hh"



class Nat624 : public Element {
  
  
 public:
  
  Nat624();
  ~Nat624();
  
  const char *class_name() const		{ return "Nat624"; }
  const char *processing() const	{ return AGNOSTIC; }
  Nat624 *clone() const { return new Nat624; }
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
  Packet *make_translate64(IPAddress src,
			   IPAddress dst,
                           unsigned short ip6_plen, 
                           unsigned char ip6_hlim, 
			   unsigned char ip6_nxt,
			   unsigned char *a);
			   
  Packet * make_icmp_translate64(unsigned char *a,
				unsigned char payload_length);

  bool lookup(IP6Address, IPAddress &);

private:

  struct Entry64 {
     IPAddress _ip4;
     IP6Address _ip6;
     bool _bound;
  };
  Vector<Entry64> _v;
  
  void add_map(IPAddress ipa4, IP6Address ipa6, bool bound);

};

#endif
