#ifndef IPSEC_DESP_HH
#define IPSEC_DESP_HH

/*
 * =c
 * IPsecESPUnencap()
 * =s encapsulation
 * removes IPSec encapsulation
 * =d
 * 
 * Removes ESP header added by IPsecESPEncap. see RFC 2406. Does not perform
 * the optional anti-replay attack checks.
 *
 * =a IPsecESPUnencap, IPsecDES, IPsecAuthSHA1
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
};

#endif

