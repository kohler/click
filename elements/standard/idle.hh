#ifndef Idle_HH
#define Idle_HH

/*
 * =c
 * Idle()
 * =d
 *
 * Has zero or more agnostic outputs and zero or more agnostic inputs. It
 * never pushes a packet to any output or pulls a packet from any input. Any
 * packet it does receive is discarded. Used to avoid "input not connected"
 * error messages.
 */

#include "element.hh"

class Idle : public Element {

 public:
  
  Idle();
  ~Idle();
  
  const char *class_name() const		{ return "Idle"; }
  void notify_ninputs(int);
  void notify_noutputs(int);
  Processing default_processing() const		{ return AGNOSTIC; }
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  
  Idle *clone() const				{ return new Idle; }
  
  void push(int, Packet *);
  Packet *pull(int);
  
};

#endif
