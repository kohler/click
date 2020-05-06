#ifndef CLICK_SETIP6FLOWLABEL_HH
#define CLICK_SETIP6FLOWLABEL_HH

/*
 * =c
 * SetIP6FlowLabel(VAL)
 * =s ip6
 * sets IP6 packets' Flow Label field
 * =d
 * Expects IP6 packets as input and
 * sets their Flow Label to VAL
 * and passes the packet to output 0.
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip6.h>
CLICK_DECLS

class SetIP6FlowLabel : public Element { public:

    SetIP6FlowLabel();
    ~SetIP6FlowLabel();

    const char *class_name() const	{ return "SetIP6FlowLabel"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    inline Packet *smaction(Packet *p);
    void push(int port, Packet *p);
    Packet *pull(int port);

  private:

    uint32_t _flow_label;/* is only 20 bits long but we cannot represent a 20-bit type */
                         /* however, we prevent from assigning this a value longer than 20-bits, */
                         /* if it still happens, we return an error */

};

CLICK_ENDDECLS
#endif
