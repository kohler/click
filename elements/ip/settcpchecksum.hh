#ifndef SETTCPCHECKSUM_HH
#define SETTCPCHECKSUM_HH

/*
 * =c
 * SetTCPChecksum()
 * =s sets TCP packets' checksums
 * =d
 * Input packets should be TCP in IP.
 *
 * Calculates the TCP header's checksum and sets the checksum header field.
 * Uses the IP header fields to generate the pseudo-header.
 *
 * =a CheckTCPHeader, SetIPChecksum, CheckIPChecksum
 */

#include "element.hh"
#include "glue.hh"

class SetTCPChecksum : public Element {
public:
  SetTCPChecksum();
  ~SetTCPChecksum();
  
  const char *class_name() const		{ return "SetTCPChecksum"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetTCPChecksum *clone() const;

  Packet *simple_action(Packet *);
};

#endif
