#ifndef SETSRCRCHECKSUM_HH
#define SETSRCRCHECKSUM_HH

/*
 * =c
 * SetSRCRChecksum()
 * =s SRCR
 * =d
 * Expects a SRCR MAC packet as input.
 * Calculates the SRCR header's checksum and sets the version and checksum header fields.
 *
 * =a
 * CheckSRCRHeader */

#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

class SetSRCRChecksum : public Element {
public:
  SetSRCRChecksum();
  ~SetSRCRChecksum();
  
  const char *class_name() const		{ return "SetSRCRChecksum"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetSRCRChecksum *clone() const;

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
