#ifndef CLICK_FORCEIP_HH
#define CLICK_FORCEIP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
CLICK_DECLS

/*
 * =c
 * ForceIP()
 * =s ip
 * Fixes fields to make packets into legal IP packets.
 * =d
 *
 * Fixes various fields in incoming packets to make sure they
 * are legal IP packets.
 *
 * =e
 *
 * RandomSource(20)
 *  -> SetIPAddress(1.2.3.4)
 *  -> StoreIPAddress(16)
 *  -> ForceIP() -> ...
 */

class ForceIP : public Element {

  int _count;

 public:

  ForceIP() CLICK_COLD;
  ~ForceIP() CLICK_COLD;

  const char *class_name() const		{ return "ForceIP"; }
  const char *port_count() const		{ return PORTS_1_1; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
