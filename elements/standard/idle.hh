#ifndef Idle_HH
#define Idle_HH

/*
 * =c
 * Idle()
 * =d
 *
 * Has zero or one outputs and zero or one inputs. It never sends anything to
 * any of them. Used to avoid "input not connected" error messages.
 */

#include "element.hh"

class Idle : public Element {
  int _idle;

 public:
  
  Idle();
  ~Idle();
  
  const char *class_name() const		{ return "Idle"; }
  void notify_ninputs(int);
  void notify_noutputs(int);
  Processing default_processing() const	{ return AGNOSTIC; }
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  
  Idle *clone() const				{ return new Idle; }
  
  void push(int, Packet *);
  Packet *pull(int);

  bool wants_packet_upstream() const;
  bool run_scheduled();
  
};

#endif
