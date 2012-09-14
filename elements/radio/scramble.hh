#ifndef SCRAMBLE_HH
#define SCRAMBLE_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * Scramble()
 *
 * Exclusive-or each packet with a fixed repeating
 * sequence of bits. The point is to try to end up
 * with a balanced number of ones and zeroes so that
 * simple transmission equipment will be happy.
 * The BIM radio, for example.
 *
 * Scramble is its own inverse.
 */

class Scramble : public Element {
public:
  Scramble() CLICK_COLD;
  ~Scramble() CLICK_COLD;

  const char *class_name() const	{ return "Scramble"; }
  const char *port_count() const	{ return PORTS_1_1; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
