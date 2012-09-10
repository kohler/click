#ifndef CLICK_ACKRESPONDER2_HH
#define CLICK_ACKRESPONDER2_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * ACKResponder2(IP)
 *
 * =s Grid
 * Send positive acknowledgements to unicast data packets.
 *
 * =d
 *
 * Input should be ACKRetrySender2 packets.  When a packet addressed
 * to IP is received on the input, the ACKRetrySender header is
 * stripped, it is passed through to output 0, and an ACK response is
 * pushed out of output 1.
 *
 * The ACK response needs to be encapsulated with this node's ethernet
 * source address, the broadcast ethernet destination address, and the
 * ACK ethernet type (typicall 0x7ffc).
 *
 * =a
 * ACKRetrySender2, ACKResponder */

class ACKResponder2 : public Element {
public:
  ACKResponder2();
  ~ACKResponder2();

  const char *class_name() const { return "ACKResponder2"; }
  const char *port_count() const { return "1/2"; }
  const char *processing() const { return PROCESSING_A_AH; }
  const char *flow_code()  const { return "x/xy"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

private:
  IPAddress _ip;
};

CLICK_ENDDECLS
#endif
