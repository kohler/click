#ifndef RADIOSIM_HH
#define RADIOSIM_HH

/*
 * =c
 * RadioSim()
 * =s
 * duplicates packets
 * V<duplication>
 * =d
 * RadioSim sends a copy of each incoming packet out each output.
 *
 * Inputs are pull, outputs are push. Services inputs in round
 * robin order.
 */

#include "element.hh"

class RadioSim : public Element {
  
 public:
  
  RadioSim();
  
  const char *class_name() const		{ return "RadioSim"; }
  const char *processing() const		{ return PULL_TO_PUSH; }
  RadioSim *clone() const;
  void notify_noutputs(int);
  void notify_ninputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  void uninitialize();

  void run_scheduled();
};

#endif
