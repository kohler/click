#ifndef CHECKGRIDHEADER_HH
#define CHECKGRIDHEADER_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * CheckGridHeader([BADADDRS])
 * =s Grid
 * =d
 * Expects Grid packets as input.
 * Checks that the packet's length is reasonable,
 * and that the Grid header length, length, and
 * checksum fields are valid.
 *
 * =a
 * SetGridChecksum
 */

class CheckGridHeader : public Element {

  int _drops;

 public:

  CheckGridHeader() CLICK_COLD;
  ~CheckGridHeader() CLICK_COLD;

  const char *class_name() const		{ return "CheckGridHeader"; }
  const char *port_count() const		{ return "1/1-2"; }
  const char *processing() const		{ return PROCESSING_A_AH; }

  int drops() const				{ return _drops; }

  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);
  void drop_it(Packet *);

};

CLICK_ENDDECLS
#endif
