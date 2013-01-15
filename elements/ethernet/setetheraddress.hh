#ifndef CLICK_SETETHERADDRESS_HH
#define CLICK_SETETHERADDRESS_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
 * =c
 * SetEtherAddress(ADDR, ANNO)
 * =s ethernet
 * sets ethernet address annotations
 * =d
 * Set the ANNO ethernet address annotation of incoming packets to the
 * static address ADDR.
 *
 * =a StoreEtherAddress, GetEtherAddress
 */

class SetEtherAddress : public Element {
  public:

    const char *class_name() const		{ return "SetEtherAddress"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:
    EtherAddress _addr;
    int _anno;
};

CLICK_ENDDECLS
#endif
