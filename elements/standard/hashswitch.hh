#ifndef HASHSWITCH_HH
#define HASHSWITCH_HH
#include <click/element.hh>

/*
 * =c
 * HashSwitch(OFFSET, LENGTH)
 * =s classification
 * classifies packets by hash of contents
 * =d
 * Can have any number of outputs.
 * Chooses the output on which to emit each packet based on
 * a hash of the LENGTH bytes starting at OFFSET.
 * Could be used for stochastic fair queuing.
 * =e
 * This element expects IP packets and chooses the output
 * based on a hash of the IP destination address:
 * 
 *   HashSwitch(16, 4)
 * =a
 * RoundRobinSwitch, StrideSwitch, Switch
 */

class HashSwitch : public Element {

  int _offset;
  int _length;
  
 public:
  
  HashSwitch();
  ~HashSwitch();
  
  const char *class_name() const		{ return "HashSwitch"; }
  const char *processing() const		{ return PUSH; }
  void notify_noutputs(int);
  
  HashSwitch *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int port, Packet *);
  
};

#endif
