#ifndef DECIPTTL_HH
#define DECIPTTL_HH

/*
 * =c
 * DecIPTTL()
 * =d
 * Expects IP packet as input.
 * If the ttl is <= 1 (i.e. has expired),
 * DecIPTTL sends the packet to output 1 (or discards it if there is no
 * output 1).
 * Otherwise it decrements the ttl, re-calculates the checksum,
 * and sends the packet to output 0.
 * 
 * Ordinarily output 1 is connected to an ICMP error packet generator.
 *
 * =e
 * This is a typical IP input processing sequence:
 *
 *   ... -> CheckIPHeader -> dt::DecIPTTL -> ...
 *   dt[1] -> ICMPError(18.26.4.24, 11, 0) -> ...
 *
 * =a ICMPError, CheckIPHeader
 */

#include "element.hh"
#include "glue.hh"

class DecIPTTL : public Element {

  int _drops;
  
 public:
  
  DecIPTTL();
  ~DecIPTTL();
  
  const char *class_name() const		{ return "DecIPTTL"; }
  const char *processing() const		{ return "a/ah"; }
  void notify_noutputs(int);
  
  int drops()					{ return _drops; }
  
  DecIPTTL *clone() const;
  void add_handlers();

  Packet *simple_action(Packet *);
  void drop_it(Packet *);
  
};

#endif
