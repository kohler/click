#ifndef IPSEC_DESP_HH
#define IPSEC_DESP_HH

/*
 * =c
 * IPsecESPUnencap
 * =s encapsulation
 * removes IPSec encapsulation
 * =d
 * 
 * removes and verifies ESP header, added by IPsecESPEncap, according to RFC
 * 2406. does not perform anti-replay attack checks.
 *
 * =a IPsecESPUnencap, IPsecDES */


#include <click/element.hh>
#include <click/glue.hh>

class DeEsp : public Element {
public:
  DeEsp();
  ~DeEsp();
  
  const char *class_name() const		{ return "IPsecESPUnencap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  DeEsp *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
private:


};

#endif



