#ifndef NETADDRESSTRANSLATOR_HH
#define NETADDRESSTRANSLATOR_HH

#include "ip6address.hh"
#include "ipaddress.hh"
#include "vector.hh"
#include "element.hh"



class NetAddressTranslator : public Element {
  
  
 public:
  
  NetAddressTranslator();
  ~NetAddressTranslator();
  
  const char *class_name() const		{ return "NetAddressTranslator"; }
  const char *processing() const	{ return AGNOSTIC; }
  NetAddressTranslator *clone() const { return new NetAddressTranslator; }
  int configure(const Vector<String> &, ErrorHandler *);
  void push(int port, Packet *p);
  //Packet *simple_action(Packet *);
  void handle_ip6(Packet *);
  void handle_ip4(Packet *);
 

  bool lookup(IP6Address, IPAddress &);
  bool reverse_lookup(IPAddress ipa4, IP6Address &ipa6);

private:

  struct Entry64 {
     IPAddress _ip4;
     IP6Address _ip6;
     bool _bound;
  };
  Vector<Entry64> _v;
  
  void add_map(IPAddress ipa4, IP6Address ipa6, bool bound);

  Packet * make_icmp_translate64(unsigned char *a,
				unsigned char payload_length);

  Packet * make_icmp_translate46(IP6Address ip6_src,
				 IP6Address ip6_dst,
				 unsigned char *a,
				 unsigned char payload_length);

  Packet * make_translate64(IPAddress src,
			   IPAddress dst,
                           unsigned short ip6_plen, 
                           unsigned char ip6_hlim, 
			   unsigned char ip6_nxt,
			   unsigned char *a);
  Packet * 
  NetAddressTranslator::make_translate46(IP6Address src, 
				         IP6Address dst,
				         unsigned short ip_len,
				         unsigned char ip_p, 
				         unsigned char ip_ttl,
					 unsigned char *a);

};
		   

#endif
