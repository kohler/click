#ifndef CLICK_FORCEUDP_HH
#define CLICK_FORCEUDP_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * ForceUDP([DPORT])
 * =s udp
 * sets UDP packet fields
 * =d
 * Set the checksum and some other fields to try to make a
 * packet look like UDP. If DPORT is specified and not -1, forces
 * the destination port to be DPORT.
 */

class ForceUDP : public Element {
public:
  ForceUDP() CLICK_COLD;
  ~ForceUDP() CLICK_COLD;

  const char *class_name() const		{ return "ForceUDP"; }
  const char *port_count() const		{ return PORTS_1_1; }
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  Packet *simple_action(Packet *);

private:
  int _count;
  int _dport;
};

CLICK_ENDDECLS
#endif
