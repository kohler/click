#ifndef CLICK_SR1GATEWAYRESPONDER_HH
#define CLICK_SR1GATEWAYRESPONDER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/wifi/linktable.hh>
#include <elements/wifi/arptable.hh>
#include <elements/wifi/path.hh>
#include <elements/wifi/sr/gatewayselector.hh>
CLICK_DECLS

/*
=c

SR1GatewayResponder(IP, ETH, ETHERTYPE, SR1GatewayResponder element, LinkTable element, ARPtable element, 
   [METRIC GridGenericMetric], [WARMUP period in seconds])

=s Wifi, Wireless Routing

Responds to queries destined for this node.

 */


class SR1GatewayResponder : public Element {
 public:
  
  SR1GatewayResponder();
  ~SR1GatewayResponder();
  
  const char *class_name() const		{ return "SR1GatewayResponder"; }
  const char *port_count() const		{ return PORTS_0_1; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/#"; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void run_timer(Timer *);
  void add_handlers();

  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint32_t _et;     // This protocol's ethertype
  unsigned int _period; // msecs
  bool _debug;

  class LinkTable *_link_table;
  class ARPTable *_arp_table;
  class GatewaySelector *_gw_sel;

  Timer _timer;
};


CLICK_ENDDECLS
#endif
