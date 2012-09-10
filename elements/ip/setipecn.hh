#ifndef CLICK_SETIPECN_HH
#define CLICK_SETIPECN_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SetIPECN(ECN)
 * =s ip
 * sets IP packets' ECN fields
 * =d
 * Expects IP packets as input and
 * sets their ECN bits to ECN.
 * This value is either a number from 0 to 3 or a keyword, namely "no",
 * "ect1"/"ECT(1)", "ect2"/"ECT(0)", or "ce"/"CE".
 * Then it incrementally recalculates the IP checksum
 * and passes the packet to output 0.
 * The ECN bits are the lower 2 bits of the IP TOS field.
 * =sa SetIPDSCP, MarkIPCE
 */

class SetIPECN : public Element { public:

    SetIPECN() CLICK_COLD;
    ~SetIPECN() CLICK_COLD;

    const char *class_name() const		{ return "SetIPECN"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    unsigned char _ecn;

};

CLICK_ENDDECLS
#endif
