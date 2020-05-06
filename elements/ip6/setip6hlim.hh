#ifndef CLICK_SETIP6HLIM_HH
#define CLICK_SETIP6HLIM_HH

/*
 * =c
 * SetIP6HLim(VAL)
 * =s ip6
 * sets IP6 packets' Hop Limit field
 * =d
 * Expects IP6 packets as input and
 * sets their Hop Limit to VAL
 * and passes the packet to output 0.
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip6.h>
CLICK_DECLS

class SetIP6Hlim : public Element { public:
    SetIP6Hlim();
    ~SetIP6Hlim();

    const char *class_name() const	{ return "SetIP6Hlim"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    inline Packet *smaction(Packet *p);
    void push(int port, Packet *p);
    Packet *pull(int port);

  private:
    uint8_t _hlim; /* field containing the hop limit */

};

CLICK_ENDDECLS
#endif /* CLICK_SETIP6HLIM_HH */
