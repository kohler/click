#ifndef ACKRESPONDER_HH
#define ACKRESPONDER_HH

/*
 * =c
 * ACKResponder(ETH)
 *
 * =s Grid
 * Send positive acknowledgements to unicast data packets.
 *
 * =d
 *
 * Input should be Ethernet packets.  When a packet is received on the
 * input, it is passed through to output 0, and an ACK response is
 * pushed out of output 1.
 * 
 * =a 
 * ACKRetrySender */


#include <click/element.hh>
#include <click/etheraddress.hh>

#define ETHERTYPE_GRID_ACK 0x7ffe

class ACKResponder : public Element {
public:
  ACKResponder();
  ~ACKResponder();

  const char *class_name() const { return "ACKResponder"; }
  const char *processing() const { return "a/ah"; }
  const char *flow_code()  const { return "x/xy"; }
  ACKResponder *clone()    const { return new ACKResponder; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);

private:
  EtherAddress _eth;
};


#endif
