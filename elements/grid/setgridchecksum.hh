#ifndef SETGRIDCHECKSUM_HH
#define SETGRIDCHECKSUM_HH

/*
 * =c
 * SetGridChecksum()
 * =s Grid
 * =d
 * Expects a Grid MAC packet as input.
 * Calculates the Grid header's checksum and sets the version and checksum header fields.
 *
 * =a
 * CheckGridHeader */

#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

class SetGridChecksum : public Element {
public:
  SetGridChecksum() CLICK_COLD;
  ~SetGridChecksum() CLICK_COLD;

  const char *class_name() const		{ return "SetGridChecksum"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
