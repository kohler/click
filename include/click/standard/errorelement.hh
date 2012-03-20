// -*- c-basic-offset: 4; related-file-name: "../../../elements/standard/errorelement.cc" -*-
#ifndef CLICK_ERRORELEMENT_HH
#define CLICK_ERRORELEMENT_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * Error(...)
 * =s debugging
 * always fails
 * =d
 * The Error element always fails to initialize. It has any number of inputs
 * and outputs, and accepts any configuration string without complaint. It is
 * useful to prevent a router from initializing while avoiding
 * spurious error messages about bad configuration strings or connections.
 * =a Message
 */

class ErrorElement : public Element { public:

    ErrorElement();

    const char *class_name() const		{ return "Error"; }
    const char *port_count() const		{ return "-/-"; }
    const char *processing() const		{ return AGNOSTIC; }
    const char *flow_code() const		{ return "x/y"; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

};

CLICK_ENDDECLS
#endif
