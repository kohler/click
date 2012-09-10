#ifndef CLICK_SETIP6DSCP_HH
#define CLICK_SETIP6DSCP_HH

/*
 * =c
 * SetIP6DSCP(VAL)
 * =s ip6
 * sets IP6 packets' DSCP fields
 * =d
 * Expects IP6 packets as input and
 * sets their Differential Services Code Point to VAL
 * and passes the packet to output 0.
 * The DSCP is the upper 6 bits of the IP6 TRAFFIC CLASS field.
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip6.h>
CLICK_DECLS

class SetIP6DSCP : public Element { public:

    SetIP6DSCP();
    ~SetIP6DSCP();

    const char *class_name() const	{ return "SetIP6DSCP"; }
    const char *port_count() const	{ return PORTS_1_1; }

    uint8_t dscp() const		{ return ntohl(_dscp) >> IP6_DSCP_SHIFT; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    inline Packet *smaction(Packet *p);
    void push(int port, Packet *p);
    Packet *pull(int port);

  private:

    uint32_t _dscp;

};

CLICK_ENDDECLS
#endif
