#ifndef CLICK_STRIPDSRHEADER_HH
#define CLICK_STRIPDSRHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * StripDSRHeader()
 * =s Grid
 * strips DSR header, saves offset to VLAN_ANNO
 * =d
 *
 * Strips the DSR header from DSR packets, saving the offset to
 * the VLAN_ANNO tag. Control packets are not stripped.
 *
 * This element helps if one wants to modify the payload inside a DSR data
 * packet. The code to parse the DSR header is taken from Click's grid
 * elements (DSRRouteTable etc).
 *
 * The IP header is set to the payload of the DSR packet.  After modifying the
 * inner IP packet, UnstripDSRHeader can be used to restore the DSR header.
 *
 * Based on Click's StripIPHeader and DSRRouteTable elements.
 *
 * =a CheckIPHeader, MarkIPHeader, StripIPHeader, DSRRouteTable
 *
 * =e
 * //DSR routing packets should go to input 1 of the DSR router
 * DSR_classifier::Classifier(09/C8,-)
 *    -> StripDSRHeader
 *    -> [... modify payload, stamp in timestamp or so ...]
 *    -> UnstripDSRHeader
 *    -> [1]dsr_rt::DSRRouteTable(...);
 */

class StripDSRHeader : public Element { public:

    StripDSRHeader() CLICK_COLD;
    ~StripDSRHeader() CLICK_COLD;

    const char *class_name() const		{ return "StripDSRHeader"; }
    const char *port_count() const		{ return PORTS_1_1; }

    Packet *simple_action(Packet *);

    Packet *swap_headers(Packet *);
};

CLICK_ENDDECLS
#endif
