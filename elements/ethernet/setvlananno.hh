#ifndef CLICK_SETVLANANNO_HH
#define CLICK_SETVLANANNO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetVlanAnno(VLAN [, VLAN_PCP])

=s ethernet

sets VLAN annotation

=d

Sets passing packets' VLAN annotations to the configured values.  VLAN must be
between 0 and 0xFFE.  VLAN_PCP defaults to 0, and must be between 0 and 7.

Return or set the ETHERTYPE parameter.

=h vlan read/write

Return or set the VLAN parameter.

=h vlan_pcp read/write

Return or set the VLAN_PCP parameter.

=a

EtherVlanEncap */

class SetVlanAnno : public Element { public:

    SetVlanAnno();
    ~SetVlanAnno();

    const char *class_name() const	{ return "SetVlanAnno"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers();

    Packet *simple_action(Packet *p);

  private:

    uint16_t _vlan_tci;

    enum { h_vlan, h_vlan_pcp };
    static String read_handler(Element *e, void *user_data);

};

CLICK_ENDDECLS
#endif
