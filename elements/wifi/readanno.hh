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

=s wifi

=d
Pulls Packet::USER_ANNO_SIZE bytes from a packet and copies
them to Packet::anno()

=e
FromDump(file) -> ReadAnno() -> xxxx

=a PushAnno
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
