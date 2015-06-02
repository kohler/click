#ifndef CPUSWITCH_HH
#define CPUSWITCH_HH
#include <click/element.hh>

/*
 * =c
 * CPUSwitch()
 * =s threads
 * classifies packets by cpu
 * =d
 * Can have any number of outputs.
 * Chooses the output on which to emit each packet based on the thread's cpu.
 * =a
 * RoundRobinSwitch, StrideSwitch, HashSwitch
 */

class CPUSwitch : public Element {

 public:

  CPUSwitch() CLICK_COLD;
  ~CPUSwitch() CLICK_COLD;

  const char *class_name() const		{ return "CPUSwitch"; }
  const char *port_count() const		{ return "1/1-"; }
  const char *processing() const		{ return PUSH; }

  void push(int port, Packet *);

};

#endif
