#ifndef FORCEIP_HH
#define FORCEIP_HH

/*
 * =c
 * ForceIP()
 * =s
 * Fixes fields to make packets into legal IP packets.
 * V<encapsulation>
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

#include <click/element.hh>
#include <click/glue.hh>
#include <click/click_ip.h>

class ForceIP : public Element {

  int _count;

 public:
  
  ForceIP();
  ~ForceIP();
  
  const char *class_name() const		{ return "ForceIP"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  ForceIP *clone() const;

  Packet *simple_action(Packet *);
  
};

#endif
