#ifndef SETSRCHECKSUM_HH
#define SETSRCHECKSUM_HH

/*
 * =c
 * SetSRChecksum()
 * =s SR
 * =d
 * Expects a SR MAC packet as input.
 * Calculates the SR header's checksum and sets the version and checksum header fields.
 *
 * =a
 * CheckSRHeader */

#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

class SetSRChecksum : public Element {
public:
  SetSRChecksum();
  ~SetSRChecksum();
  
  const char *class_name() const		{ return "SetSRChecksum"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetSRChecksum *clone() const;

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
