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
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void push(int, Packet *);
  Packet *pull(int);

  void run_scheduled();
  
};

#endif
