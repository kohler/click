#ifndef CLICK_TCPACK_HH
#define CLICK_TCPACK_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

/*
 * =c
 * TCPAck([ACK_DELAY])
 * =s tcp
 * acknowledge TCP packets
 * =d
 *
 * performs TCP style acknowledgement. marked TCP/IP packets are expected on
 * both input and output ports. use MarkIPHeader to mark packets.
 *
 * input port 0 is TCP input. a packet that arrives on this input may trigger
 * an acknowledgement. the acknowledgement acknowledges the latest in-order
 * packet received. TCPAck gets this information from a downstream TCPBuffer
 * element that it discovers using flow-based router context, much like how
 * RED elements discover their QUEUE elements. the packet is sent out on
 * output port 0 untouched.
 *
 * input port 1 is TCP output. a packet that arrives on this input get tagged
 * with an acknowledgement number. this ack number is obtained from TCPBuffer
 * as described above. this packet also causes TCPAck to cancel scheduled ACK.
 * the packet is then sent out on output port 1.
 *
 * finally, output port 2 is used to send scheduled ACKs. packets generated on
 * this port does not have any of the flow ID nor sequence number. another
 * element, such as TCPConn, should be used downstream from this port to set
 * those fields. an ACK is generated on this output only if after ACK_DELAY
 * number of ms a triggered acknowledge was not sent. by default, ACK_DELAY is
 * set to 20.
 *
 * TCPAck only deals with DATA packets. it doesn't try to acknowledge SYN and
 * FIN packets. TCPAck starts using ack number from the first SYN ACK packet
 * it sees on in/output port 0 or 1. packets before that are rejected.
 *
 * TCPAck does not compute checksum on any packets. use SetIPChecksum and
 * SetTCPChecksum instead.
 */

class TCPBuffer;

class TCPAck : public Element {
private:
  Timer _timer;

  bool _synack;
  bool _needack;
  unsigned _ack_nxt;

  TCPBuffer *_tcpbuffer;

  unsigned _ackdelay_ms;

  bool iput(Packet *);
  bool oput(Packet *);
  void send_ack();

public:
  TCPAck() CLICK_COLD;
  ~TCPAck() CLICK_COLD;

  const char *class_name() const		{ return "TCPAck"; }
  const char *port_count() const		{ return "2/3"; }
  const char *processing() const		{ return "aa/aah"; }

  int initialize(ErrorHandler *) CLICK_COLD;
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  void push(int, Packet *);
  Packet *pull(int);
  void run_timer(Timer *);
};

CLICK_ENDDECLS
#endif
