#ifndef CLICK_SETTCPCHECKSUM_HH
#define CLICK_SETTCPCHECKSUM_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * SetTCPChecksum([FIXOFF])
 * =s tcp
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

  SetTCPChecksum() CLICK_COLD;
  ~SetTCPChecksum() CLICK_COLD;

  const char *class_name() const		{ return "SetTCPChecksum"; }
  const char *port_count() const		{ return PORTS_1_1; }
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  Packet *simple_action(Packet *);

private:
  bool _fixoff;
};

CLICK_ENDDECLS
#endif
