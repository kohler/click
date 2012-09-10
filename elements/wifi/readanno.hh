#ifndef CLICK_READANNO_HH
#define CLICK_READANNO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
ReadAnno([I<KEYWORDS>])

=s Wifi
Pulls annotation from packet and stores it in Packet::anno()

=d
Pulls Packet::USER_ANNO_SIZE bytes from a packet and copies
them to Packet::anno()

=e
FromDump(file) -> ReadAnno() -> xxxx

=a PushAnno
*/


class ReadAnno : public Element { public:

  ReadAnno() CLICK_COLD;
  ~ReadAnno() CLICK_COLD;

  const char *class_name() const		{ return "ReadAnno"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return "a/a"; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
