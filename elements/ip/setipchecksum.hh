#ifndef SETIPCHECKSUM_HH
#define SETIPCHECKSUM_HH

/*
 * =c
 * SetIPChecksum()
 * =d
 * Expects an IP packet as input.
 * Calculates and sets the IP header checksum.
 *
 * This element is mostly used after DecIPTTL,
 * which modifies the IP header.
 * 
 * =a CheckIPChecksum
 * =a DecIPTTL
 */

#include "element.hh"
#include "glue.hh"

class SetIPChecksum : public Element {
public:
  SetIPChecksum();
  ~SetIPChecksum();
  
  const char *class_name() const		{ return "SetIPChecksum"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  SetIPChecksum *clone() const;

  Packet *simple_action(Packet *);
};

#endif
