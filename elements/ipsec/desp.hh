#ifndef CLICK_IPSEC_DESP_HH
#define CLICK_IPSEC_DESP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include "satable.hh"
#include "sadatatuple.hh"
CLICK_DECLS

/*
 * =c
 * IPsecESPUnencap()
 * =s ipsec
 * removes IPSec encapsulation
 * =d
 *
 * Removes ESP header added by IPsecESPEncap. see RFC 2406. Does not perform
 * the optional anti-replay attack checks.
 *
 * =a IPsecESPUnencap, IPsecDES, IPsecAuthSHA1
 */

class IPsecESPUnencap : public Element {
public:
  IPsecESPUnencap() CLICK_COLD;
  ~IPsecESPUnencap() CLICK_COLD;

  const char *class_name() const	{ return "IPsecESPUnencap"; }
  const char *port_count() const	{ return PORTS_1_1; }

  int checkreplaywindow(SADataTuple * sa_data,unsigned long seq);

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
