#ifndef PROTOCOLTRANSLATOR_HH
#define PROTOCOLTRANSLATOR_HH

#include <click/ip6address.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/element.hh>



class ProtocolTranslator : public Element {
  
  
 public:
  
  ProtocolTranslator();
  ~ProtocolTranslator();
  
  const char *class_name() const		{ return "ProtocolTranslator"; }
  const char *processing() const	{ return AGNOSTIC; }
  ProtocolTranslator *clone() const { return new ProtocolTranslator; }
  int configure(const Vector<String> &, ErrorHandler *);
  void push(int port, Packet *p);
  //Packet *simple_action(Packet *);
  void handle_ip6(Packet *);
  void handle_ip4(Packet *);
 
private:

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
  Packet * make_translate46(IP6Address src, 
			    IP6Address dst,
			    unsigned short ip_len,
			    unsigned char ip_p, 
			    unsigned char ip_ttl,
			    unsigned char *a);

};
		   

#endif
