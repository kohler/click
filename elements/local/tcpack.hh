#ifndef TCPACK_HH
#define TCPACK_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/click_tcp.h>

/*
 * =c
 * TCPAck([ACK_DELAY])
 * =s TCP
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
 * finally, output port 2 is used to send scheduled ACKs. TCPAck uses the
 * latest seq number it sees across input/output port 1 as the sequence number
 * for the acknowledgement. an ACK is generated on this output only if after
 * ACK_DELAY number of ms a triggered acknowledge was not sent. by default,
 * ACK_DELAY is set to 20.
 *
 * TCPAck only deals with DATA packets. it doesn't try to acknowledge SYN and
 * FIN packets. TCPAck starts using ack number from the first SYN ACK packet
 * it sees on in/output port 0 or 1. packets before that are rejected. the tcp
 * and ip header from this packet are used to send explicit ACK packets. 
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
  bool _copyhdr;
  unsigned _seq_nxt;
  unsigned _ack_nxt;
  
  click_tcp _tcph_in;
  click_ip  _iph_in;

  TCPBuffer *_tcpbuffer;

  unsigned _ackdelay_ms;
  
  bool iput(Packet *);
  bool oput(Packet *);
  void send_ack();

public:
  TCPAck();
  ~TCPAck();
  
  const char *class_name() const		{ return "TCPAck"; }
  const char *processing() const		{ return "aa/aah"; }
  TCPAck *clone() const				{ return new TCPAck; }

  int initialize(ErrorHandler *);
  void uninitialize();
  int configure(const Vector<String> &conf, ErrorHandler *errh);

  void push(int, Packet *);
  Packet *pull(int);
  void run_scheduled();
};

#endif

