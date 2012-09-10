#ifndef CLICK_SETIPDSCP_HH
#define CLICK_SETIPDSCP_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * SetIPDSCP(DSCP)
 * =s ip
 * sets IP packets' DSCP fields
 * =d
 * Expects IP packets as input and
 * sets their Differential Services Code Point to DSCP.
 * Then it incrementally recalculates the IP checksum
 * and passes the packet to output 0.
 * The DSCP is the upper 6 bits of the IP TOS field.
 * =sa SetIPECN
 */

class SetIPDSCP : public Element { public:

  SetIPDSCP() CLICK_COLD;
  ~SetIPDSCP() CLICK_COLD;

  const char *class_name() const		{ return "SetIPDSCP"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }
  void add_handlers() CLICK_COLD;

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

  private:

    unsigned char _dscp;

};

CLICK_ENDDECLS
#endif
