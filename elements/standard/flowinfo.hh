// -*- c-basic-offset: 4 -*-
#ifndef CLICK_FLOWINFO_HH
#define CLICK_FLOWINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

FlowInfo(ELEMENT FLOWCODE, ...)

=s information

specifies flow codes

=io

None

=d

Allows the user to override specific elements' flow codes. Each configuration
argument has the form `ELEMENT CODE', meaning that the element named ELEMENT
has flow code CODE.
*/

class FlowInfo : public Element { public:

    FlowInfo();

    const char* class_name() const	{ return "FlowInfo"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }
    int configure(Vector<String>&, ErrorHandler*);

};

CLICK_ENDDECLS
#endif
