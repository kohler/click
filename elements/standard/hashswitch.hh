#ifndef CLICK_HASHSWITCH_HH
#define CLICK_HASHSWITCH_HH
#include <click/element.hh>
CLICK_DECLS

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
 * Switch, RoundRobinSwitch, StrideSwitch, RandomSwitch
 */

class HashSwitch : public Element {

  int _offset;
  int _length;

 public:

  HashSwitch() CLICK_COLD;

  const char *class_name() const		{ return "HashSwitch"; }
  const char *port_count() const		{ return "1/1-"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  void push(int port, Packet *);

};

CLICK_ENDDECLS
#endif
