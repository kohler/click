#ifndef CLICK_ACKRESPONDER_HH
#define CLICK_ACKRESPONDER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

/*
 * =c
 * ACKResponder(ETH)
 *
 * =s Grid
 * Send positive acknowledgements to unicast data packets.
 *
 * =d
 *
 * Input should be Ethernet packets.  When a packet addressed to ETH
 * is received on the input, it is passed through to output 0, and an
 * ACK response is pushed out of output 1.
 *
 * =a
 * ACKRetrySender */

#define ETHERTYPE_GRID_ACK 0x7ffa

class ACKResponder : public Element {
public:
  ACKResponder() CLICK_COLD;
  ~ACKResponder() CLICK_COLD;

  const char *class_name() const { return "ACKResponder"; }
  const char *port_count() const { return "1/2"; }
  const char *processing() const { return PROCESSING_A_AH; }
  const char *flow_code()  const { return "x/xy"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

private:
  EtherAddress _eth;
};

CLICK_ENDDECLS
#endif
