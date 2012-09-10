#ifndef CLICK_SETVLANANNO_HH
#define CLICK_SETVLANANNO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SetVLANAnno(VLAN_TCI [, VLAN_PCP])

=s ethernet

sets VLAN annotation

=d

Sets passing packets' VLAN_TCI annotations to the configured values.  Set the
annotation using VLAN_TCI (the whole annotation), or VLAN_ID and VLAN_PCP
(setting the ID and Priority Code Point components, respectively).

=h vlan_tci read/write

Return or set the VLAN_TCI parameter.

=h vlan_id read/write

Return or set the VLAN_ID parameter.

=h vlan_pcp read/write

Return or set the VLAN_PCP parameter.

=a

EtherVLANEncap */

class SetVLANAnno : public Element { public:

    SetVLANAnno() CLICK_COLD;
    ~SetVLANAnno() CLICK_COLD;

    const char *class_name() const	{ return "SetVLANAnno"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *p);

  private:

    uint16_t _vlan_tci;

    enum { h_config, h_vlan_tci };
    static String read_handler(Element *e, void *user_data) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
