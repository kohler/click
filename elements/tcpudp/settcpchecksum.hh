#ifndef CLICK_SETTCPCHECKSUM_HH
#define CLICK_SETTCPCHECKSUM_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * SetTCPChecksum([FIX-OFF?])
 * =s TCP
 * sets TCP packets' checksums
 * =d
 * Input packets should be TCP in IP.
 *
 * Calculates the TCP header's checksum and sets the checksum header field.
 * Uses the IP header fields to generate the pseudo-header.
 *
 * =a CheckTCPHeader, SetIPChecksum, CheckIPHeader, SetUDPChecksum
 */

class SetTCPChecksum : public Element { public:
  
  SetTCPChecksum();
  ~SetTCPChecksum();
  
  const char *class_name() const		{ return "SetTCPChecksum"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetTCPChecksum *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);

  Packet *simple_action(Packet *);

private:
  bool _fixoff;
};

CLICK_ENDDECLS
#endif
