#ifndef CPUSWITCH_HH
#define CPUSWITCH_HH
#include <click/element.hh>

/*
 * =c
 * CPUSwitch()
 * =s classification
 * classifies packets by cpu
 * =d
 * Can have any number of outputs.
 * Chooses the output on which to emit each packet based on the thread's cpu.
 * =a
 * RoundRobinSwitch, StrideSwitch, HashSwitch
 */

class CPUSwitch : public Element {

 public:
  
  CPUSwitch();
  ~CPUSwitch();
  
  const char *class_name() const		{ return "CPUSwitch"; }
  const char *processing() const		{ return PUSH; }
  void notify_noutputs(int);
  
  CPUSwitch *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int port, Packet *);
  
};

#endif
