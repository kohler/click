#ifndef CLICK_FloodTracker_HH
#define CLICK_FloodTracker_HH
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
CLICK_DECLS

/*
=c
FloodTracker(IP, ETH, ETHERTYPE, LinkTable, ARPTable, 
                [PERIOD timeout], [GW is_gateway], 
                [METRIC GridGenericMetric])

=s Wifi, Wireless Routing

Select a gateway to send a packet to based on TCP connection
state and metric to gateway.

=d

Input 0: packets from dev
Input 1: packets for gateway node
Output 0: packets to dev
Output 1: packets with dst_ip anno set

This element provides proactive gateway selection.  
Each gateway broadcasts an ad every PERIOD seconds.  
Non-gateway nodes select the gateway with the best metric
and forward ads.

 */


class FloodTracker : public Element {
 public:
  
  FloodTracker();
  ~FloodTracker();
  
  const char *class_name() const		{ return "FloodTracker"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  int configure(Vector<String> &conf, ErrorHandler *errh);

  /* handler stuff */
  void add_handlers();

  String print_gateway_stats();

  static String read_param(Element *, void *);

  Packet *simple_action(Packet *);
private:
    // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _ip;
    u_long _seq;
    int _count;
    Timestamp _when; /* when we saw the first query */
    Seen(IPAddress ip, u_long seq) {
	_ip = ip; 
	_seq = seq; 
	_count = 0;
    }
    Seen();
  };
  
  DEQueue<Seen> _seen;

  class IPInfo {
  public:
    IPAddress _ip;
    Timestamp _first_update;
    Timestamp _last_update;
    int _seen;
    IPInfo() {memset(this,0,sizeof(*this)); }
  };

  typedef HashMap<IPAddress, IPInfo> IPTable;
  typedef IPTable::const_iterator IPIter;
  IPTable _gateways;
};


CLICK_ENDDECLS
#endif
