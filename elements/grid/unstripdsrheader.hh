#ifndef CLICK_UNSTRIPDSRHEADER_HH
#define CLICK_UNSTRIPDSRHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * UnstripDSRHeader()
 * =s ip
 * restores DSR Header
 * =d
 *
 * Undoes StripDSRHeader: Swaps the IP and the DSR header a second time,
 * pushes the dsr_len offset previously saved to VLAN_ANNO.
 *
 * =a CheckDSRHeader, MarkDSRHeader, StripDSRHeader */

class UnstripDSRHeader : public Element { public:

  UnstripDSRHeader() CLICK_COLD;
  ~UnstripDSRHeader() CLICK_COLD;

  const char *class_name() const		{ return "UnstripDSRHeader"; }
  const char *port_count() const		{ return PORTS_1_1; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
