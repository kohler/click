#ifndef CLICK_FORCEICMP_HH
#define CLICK_FORCEICMP_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * ForceICMP([TYPE, CODE])
 * =s icmp
 * sets ICMP checksum
 * =d
 * Sets the ICMP checksum of an ICMP-in-IP packet. Optionally
 * sets the TYPE and CODE of the ICMP header.
 */

class ForceICMP : public Element {
public:
  ForceICMP() CLICK_COLD;
  ~ForceICMP() CLICK_COLD;

  const char *class_name() const		{ return "ForceICMP"; }
  const char *port_count() const		{ return PORTS_1_1; }
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  Packet *simple_action(Packet *);

private:
  int _count;
  int _type;
  int _code;
};

CLICK_ENDDECLS
#endif
