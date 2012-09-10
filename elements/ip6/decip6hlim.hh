#ifndef CLICK_DECIP6HLIM_HH
#define CLICK_DECIP6HLIM_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * DecIP6HLIM()
 * =s ip6
 *
 * =d
 * Expects IP6 packet as input.
 * If the hlim is <= 1 (i.e. has expired),
 * DecIP6HLIM sends the packet to output 1 (or discards it if there is no
 * output 1).
 * Otherwise it decrements the hlim,
 * and sends the packet to output 0.
 *
 * Ordinarily output 1 is connected to an ICMP6 error packet generator.
 *
 * =e
 * This is a typical IP6 input processing sequence:
 *
 *   ... -> CheckIP6Header -> dt::DecIP6HLIM -> ...
 *   dt[1] -> ICMP6Error(...) -> ...
 *
 * =a ICMP6Error, CheckIP6Header
 */

class DecIP6HLIM : public Element {

  int _drops;

 public:

  DecIP6HLIM();
  ~DecIP6HLIM();

  const char *class_name() const		{ return "DecIP6HLIM"; }
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PROCESSING_A_AH; }

  int drops()					{ return _drops; }

  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);
  void drop_it(Packet *);

};

CLICK_ENDDECLS
#endif
