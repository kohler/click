#ifndef DECIPTTL_HH
#define DECIPTTL_HH

/*
 * =c
 * DecIPTTL
 * =s IP
 * decrements IP time-to-live, drops dead packets
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

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>

class DecIPTTL : public Element { public:

  DecIPTTL();
  ~DecIPTTL();
  
  const char *class_name() const		{ return "DecIPTTL"; }
  const char *processing() const		{ return "a/ah"; }
  void notify_noutputs(int);
  
  u_int32_t drops()				{ return _drops; }
  
  DecIPTTL *clone() const;
  void add_handlers();

  Packet *simple_action(Packet *);
  void drop_it(Packet *);
  
 private:

  u_atomic32_t _drops;
  
};

#endif
