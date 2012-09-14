#ifndef CLICK_MARKMACHEADER_HH
#define CLICK_MARKMACHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * MarkMACHeader([OFFSET, LENGTH])
 * =s ip
 * sets MAC header annotation
 * =d
 *
 * Sets the MAC header annotation to start OFFSET bytes into the packet.
 * OFFSET defaults to 0.  If LENGTH is provided and non-zero, the MAC header
 * is LENGTH bytes long, so the network header annotation is set to
 * OFFSET+LENGTH bytes into the packet.  The packet must be long enough to
 * support the provided OFFSET.
 *
 * =a MarkIPHeader */

class MarkMACHeader : public Element { public:

    MarkMACHeader() CLICK_COLD;

    const char *class_name() const		{ return "MarkMACHeader"; }
    const char *port_count() const		{ return PORTS_1_1; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }

    Packet *simple_action(Packet *);

  private:

    int _offset;
    int _length;

};

CLICK_ENDDECLS
#endif
