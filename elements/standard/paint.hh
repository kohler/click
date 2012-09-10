#ifndef CLICK_PAINT_HH
#define CLICK_PAINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Paint(COLOR [, ANNO])

=s paint

sets packet paint annotations

=d

Sets each packet's paint annotation to COLOR, an integer 0..255.

Paint sets the packet's PAINT annotation by default, but the ANNO argument can
specify any one-byte annotation.

=h color read/write

Get/set the color to paint.

=a PaintTee */

class Paint : public Element { public:

    Paint() CLICK_COLD;

    const char *class_name() const		{ return "Paint"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    uint8_t _anno;
    uint8_t _color;

};

CLICK_ENDDECLS
#endif
