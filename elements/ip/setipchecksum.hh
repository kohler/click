#ifndef SETIPCHECKSUM_HH
#define SETIPCHECKSUM_HH

/*
 * =c
 * SetIPChecksum()
 * =d
 * Expects an IP packet as input.
 * Calculates the IP header's checksum and sets the checksum header field.
 *
 * You will not normally need SetIPChecksum. Most elements that modify an IP
 * header, like DecIPTTL, SetIPDSCP, and Rewriter, already update the checksum
 * incrementally.
 *
 * =a CheckIPHeader DecIPTTL SetIPDSCP Rewriter */

#include "element.hh"
#include "glue.hh"

class SetIPChecksum : public Element {
public:
  SetIPChecksum();
  ~SetIPChecksum();
  
  const char *class_name() const		{ return "SetIPChecksum"; }
  const char *processing() const	{ return AGNOSTIC; }
  SetIPChecksum *clone() const;

  Packet *simple_action(Packet *);
};

#endif
