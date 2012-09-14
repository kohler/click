#ifndef CLICK_TCPDEMUX_HH
#define CLICK_TCPDEMUX_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <clicknet/tcp.h>
#include <click/bighashmap.hh>
#include <click/ipflowid.hh>
CLICK_DECLS

/*
 * =c
 * TCPDemux()
 * =s tcp
 * demultiplexes TCP flows
 * =d
 *
 * for now,
 *
 * output 0 = SYN
 * output 1 = FIN | RST
 * output 2 = others
 */

typedef HashMap<IPFlowID, int> FlowTable;

class TCPDemux : public Element {
private:
  FlowTable _flows;
  int find_flow(Packet *p);

public:
  TCPDemux() CLICK_COLD;
  ~TCPDemux() CLICK_COLD;

  const char *class_name() const		{ return "TCPDemux"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  void push(int, Packet *);

  // add new flow to flow table; returns false if key already exists, true
  // otherwise. entry is not added to table in the former case
  bool add_flow(IPAddress sa, unsigned short sp,
                IPAddress da, unsigned short dp, unsigned port);

  // remove flow from table; returns true if removed an entry, false otherwise
  bool remove_flow(IPAddress sa, unsigned short sp,
                   IPAddress da, unsigned short dp);
};

CLICK_ENDDECLS
#endif
