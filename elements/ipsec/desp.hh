#ifndef IPSEC_DESP_HH
#define IPSEC_DESP_HH

/*
 * =c
 * IPsecESPUnencap
 * =s encapsulation
 * removes IPSec encapsulation
 * =d
 * 
 * Removes and verifies ESP header added by IPsecESPEncap. see RFC 2406. does
 * not perform anti-replay attack checks. If DES-CBC encryption and decryption
 * are used, IPsecDES must be used before IPsecESPUnencap.
 *
 * =a IPsecESPUnencap, IPsecDES 
 */

#include <click/element.hh>
#include <click/glue.hh>

class IPsecESPUnencap : public Element {
public:
  IPsecESPUnencap();
  ~IPsecESPUnencap();
  
  const char *class_name() const	{ return "IPsecESPUnencap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  IPsecESPUnencap *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
private:


};

#endif

