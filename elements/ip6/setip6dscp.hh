#ifndef CLICK_SETIP6DSCP_HH
#define CLICK_SETIP6DSCP_HH

/*
 * =c
 * SetIP6DSCP(VAL)
 * =s IP6
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

class SetIP6DSCP : public Element {

  uint32_t _dscp;
  
 public:
  
  SetIP6DSCP();
  ~SetIP6DSCP();
  
  const char *class_name() const	{ return "SetIP6DSCP"; }
  const char *processing() const	{ return AGNOSTIC; }
  SetIP6DSCP *clone() const;

  uint8_t dscp() const			{ return _dscp >> IP6_DSCP_SHIFT; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers();

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

CLICK_ENDDECLS
#endif
