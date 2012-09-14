#ifndef CLICK_TCPCONN_HH
#define CLICK_TCPCONN_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <clicknet/tcp.h>
#include <click/bighashmap.hh>
#include <click/ipflowid.hh>
CLICK_DECLS

/*
 * =c
 * TCPConn()
 * =s tcp
 * manage tcp connections
 * =d
 * can either start a tcp connection or listen for connections.
 *
 * input and output 0 carry incoming packets; input and output 1 carry
 * outgoing packets; output 2 send out SYNs and SYN ACKs. incoming packets are
 * pushed onto input 0, but outgoing packets are pulled thru input/output 1.
 * does not allow output pull to succeed unless connection is established.
 */

class TCPDemux;

class TCPConn : public Element {
private:
  TCPDemux *_tcpdemux;

  bool _active;
  bool _listen;
  bool _established;

  unsigned _seq_nxt;
  IPFlowID _flow;

  bool connect_handler(IPFlowID f);
  bool listen_handler(IPFlowID f);
  void send_syn();

  void reset();
  bool iput(Packet *p);
  Packet *oput(Packet *p);

public:
  TCPConn() CLICK_COLD;
  ~TCPConn() CLICK_COLD;

  const char *class_name() const		{ return "TCPConn"; }
  const char *port_count() const		{ return "2/3"; }
  const char *processing() const		{ return "hl/hlh"; }

  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  void push(int, Packet *);
  Packet *pull(int);
  void add_handlers() CLICK_COLD;

  static int ctrl_write_handler
    (const String &, Element *, void *, ErrorHandler *);
  static int reset_write_handler
    (const String &, Element *, void *, ErrorHandler *);
};

CLICK_ENDDECLS
#endif

