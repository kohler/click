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
 * = ... -> CheckIPChecksum -> dt::DecIPTTL -> ...
 * = dt[1] -> ICMPError(18.26.4.24, 11, 0) -> ...
 *
 * =a ICMPError
 * =a CheckIPChecksum
 */

#include "element.hh"
#include "glue.hh"

class DecIPTTL : public Element {

  int _drops;
  
 public:
  
  DecIPTTL();
  ~DecIPTTL();
  
  const char *class_name() const		{ return "DecIPTTL"; }
  void notify_outputs(int);
  void processing_vector(Vector<int> &, int, Vector<int> &, int) const;
  
  int drops()					{ return _drops; }
  
  DecIPTTL *clone() const;
  void add_handlers(HandlerRegistry *fcr);

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif
