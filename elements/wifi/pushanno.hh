#ifndef CLICK_PUSHANNO_HH
#define CLICK_PUSHANNO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * PushAnno([I<KEYWORDS>])
 *
 * =s wifi
 * Aggregates large packets from the madwifi.stripped driver.
 * Assumes fragemented packets are sent in order, and
 * the the WIFI_RX_MORE_ANNO is set to a non-zero value
 * if the next packet contains more fragments.
 * 
 * usual configurations look like:
 * FromDevice(ath0) -> PushAnno() -> xxxx
 * 
 * =s wifi
 *
 */


class PushAnno : public Element { public:
  
  PushAnno();
  ~PushAnno();
  
  const char *class_name() const		{ return "PushAnno"; }
  const char *processing() const		{ return "a/a"; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);  

};

CLICK_ENDDECLS
#endif
