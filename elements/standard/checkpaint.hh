#ifndef CLICK_CHECKPAINT_HH
#define CLICK_CHECKPAINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

CheckPaint(COLOR [, ANNO])

=s paint

checks packets' paint annotation

=d

Checks that incoming packets have paint annotation equal to COLOR. If their
paints are not equal to COLOR, then they are dropped or emitted on output 1,
depending on how many outputs were used.

CheckPaint uses the packet's PAINT annotation by default, but the ANNO
argument can specify any one-byte annotation.

=a Paint, PaintTee */

class CheckPaint : public Element { public:

    CheckPaint() CLICK_COLD;

    const char *class_name() const	{ return "CheckPaint"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void push(int, Packet *);
    Packet *pull(int);

  private:

    uint8_t _anno;
    uint8_t _color;

};

CLICK_ENDDECLS
#endif
