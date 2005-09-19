#ifndef CLICK_STRIPIPHEADER_HH
#define CLICK_STRIPIPHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * StripIPHeader()
 * =s encapsulation, IP
 * strips outermost IP header
 * =d
 * Removes the outermost IP header from IP packets based on the IP Header annotation.
 *
 * =a CheckIPHeader, CheckIPHeader2, MarkIPHeader, UnstripIPHeader, Strip
 */

class StripIPHeader : public Element { public:
  
    StripIPHeader();
    ~StripIPHeader();
  
    const char *class_name() const		{ return "StripIPHeader"; }
    const char *port_count() const		{ return PORTS_1_1; }

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
