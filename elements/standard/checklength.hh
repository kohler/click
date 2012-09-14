#ifndef CLICK_CHECKLENGTH_HH
#define CLICK_CHECKLENGTH_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

CheckLength(LENGTH)

=s classification

drops large packets

=d

CheckLength checks every packet's length against LENGTH. If the packet is
no larger than LENGTH, it is sent to output 0; otherwise, it is sent to
output 1 (or dropped if there is no output 1).
*/

class CheckLength : public Element { public:

  CheckLength() CLICK_COLD;

  const char *class_name() const		{ return "CheckLength"; }
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PROCESSING_A_AH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  void push(int, Packet *);
  Packet *pull(int);

  void add_handlers() CLICK_COLD;

 protected:

  unsigned _max;

};

CLICK_ENDDECLS
#endif
