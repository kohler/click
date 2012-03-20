// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TRUNCATE_HH
#define CLICK_TRUNCATE_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * Truncate(LENGTH)
 * =s basicmod
 * limit packet length.
 * =d
 * Shorten packets to at most LENGTH bytes.
 *
 * The EXTRA_LENGTH keyword argument determines whether packets' extra length
 * annotations are updated to account for any dropped bytes.  Default is true.
 * =a Strip
 */

class Truncate : public Element { public:

    Truncate();

    const char *class_name() const		{ return "Truncate"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }

    Packet *simple_action(Packet *);

    void add_handlers();

  private:

    unsigned _nbytes;
    bool _extra_anno;

};

CLICK_ENDDECLS
#endif
