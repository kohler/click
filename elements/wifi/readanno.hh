#ifndef CLICK_READANNO_HH
#define CLICK_READANNO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * ReadAnno([I<KEYWORDS>])
 *
 * =s wifi
 * Aggregates large packets from the madwifi.readped driver.
 * Assumes fragemented packets are sent in order, and
 * the the WIFI_RX_MORE_ANNO is set to a non-zero value
 * if the next packet contains more fragments.
 * 
 * usual configurations look like:
 * FromDevice(ath0) -> ReadAnno() -> xxxx
 * 
 * =s wifi
 *
 */


class ReadAnno : public Element { public:
  
  ReadAnno();
  ~ReadAnno();
  
  const char *class_name() const		{ return "ReadAnno"; }
  const char *processing() const		{ return "a/a"; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);  

};

CLICK_ENDDECLS
#endif
