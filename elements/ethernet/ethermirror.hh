#ifndef ETHERMIRROR_HH
#define ETHERMIRROR_HH
#include <click/element.hh>

/*
 * =c
 * EtherMirror()
 * =s swaps Ethernet source and destination
 * V<modifies>
 * =d
 *
 * Incoming packets are Ethernet. Their source and destination Ethernet
 * addresses are swapped before they are output.
 * */

class EtherMirror : public Element {

 public:

  EtherMirror() 				{ add_input(); add_output(); }
  EtherMirror *clone() const			{ return new EtherMirror(); }
  
  const char *class_name() const		{ return "EtherMirror"; }
  
  Packet *simple_action(Packet *);
  
};

#endif ETHERMIRROR_HH
