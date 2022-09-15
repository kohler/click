#ifndef CLICK_SETIP6ECN_HH
#define CLICK_SETIP6ECN_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SetIP6ECN(ECN)
 * =s ip6
 * sets IPv6 packets' ECN fields
 * =d
 * Expects IPv6 packets as input and
 * sets their ECN bits to ECN.
 * This value is either a number from 0 to 3 or a keyword, namely "no",
 * "ect1"/"ECT(1)", "ect2"/"ECT(0)", or "ce"/"CE".
 * Then it passes the packet to output 0.
 * The ECN bits are the 2 least-significant bits in the Traffic Class field.
 * =sa SetIPDSCP, MarkIPCE
 */

class SetIP6ECN : public Element { public:

    SetIP6ECN() CLICK_COLD;
    ~SetIP6ECN() CLICK_COLD;

    const char *class_name() const		{ return "SetIP6ECN"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    uint32_t _ecn;

};

CLICK_ENDDECLS
#endif
