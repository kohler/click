#ifndef CLICK_PUSHANNO_HH
#define CLICK_PUSHANNO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c 
PushAnno([I<KEYWORDS>])
=s wifi
=d
Pushes Packet::USER_ANNO_SIZE bytes on a packet and copies
Packet::anno() to the beginning of the packet.

=e
FromDevice(ath0) -> PushAnno() -> ToDump()

=a ReadAnno
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
