
#ifndef TCPDEMUX_HH
#define TCPDEMUX_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/click_tcp.h>
#include <click/bighashmap.hh>
#include <click/ipflowid.hh>

/*
 * =c
 * TCPDemux()
 * =s TCP
 * demultiplexes TCP flows
 * =d
 *
 * for now, 
 *
 * output 0 = SYN
 * output 1 = FIN | RST
 * output 2 = others
 */

typedef BigHashMap<IPFlowID, int> FlowTable;

class TCPDemux : public Element {
private:
  FlowTable _flows;
  int find_flow(Packet *p);

public:
  TCPDemux();
  ~TCPDemux();
  
  const char *class_name() const		{ return "TCPDemux"; }
  const char *processing() const		{ return PUSH; }
  TCPDemux *clone() const			{ return new TCPDemux; }

  int initialize(ErrorHandler *);
  void uninitialize();
  int configure(const Vector<String> &conf, ErrorHandler *errh);

  void push(int, Packet *);

  // add new flow to flow table; returns false if key already exists, true
  // otherwise. entry is not added to table in the former case
  bool add_flow(IPAddress sa, unsigned short sp, 
                IPAddress da, unsigned short dp, unsigned port);

  // remove flow from table; returns true if removed an entry, false otherwise
  bool remove_flow(IPAddress sa, unsigned short sp, 
                   IPAddress da, unsigned short dp);
};

#endif

