#ifndef CLICK_PAINTTEE_HH
#define CLICK_PAINTTEE_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * PaintTee(COLOR [, ANNO])
 * =s paint
 * duplicates packets with given paint annotation
 * =d
 *
 * PaintTee sends every packet through output 0. If the packet's
 * paint annotation is equal to COLOR (an integer), it also
 * sends a copy through output 1.
 *
 * PaintTee uses the PAINT annotation by default, but the ANNO argument can
 * specify any one-byte annotation.
 *
 * =e
 * Intended to produce redirects in conjunction with Paint and
 * ICMPError as follows:
 *
 *   FromDevice(eth7) -> Paint(7) -> ...
 *   routingtable[7] -> pt :: PaintTee(7) -> ... -> ToDevice(eth7)
 *   pt[1] -> ICMPError(18.26.4.24, 5, 1) -> [0]routingtable;
 *
 * =a Paint, ICMPError
 */

class PaintTee : public Element { public:

    PaintTee();

    const char *class_name() const	{ return "PaintTee"; }
    const char *port_count() const	{ return "1/2"; }
    const char *processing() const	{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    Packet *simple_action(Packet *);

  private:

    uint8_t _anno;
    uint8_t _color;

};

CLICK_ENDDECLS
#endif
