#ifndef CLICK_SRQUERIER_HH
#define CLICK_SRQUERIER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/wifi/linktable.hh>
#include <elements/wifi/sr/path.hh>
#include "srquerier.hh"
CLICK_DECLS

/*

=c

SRQuerier(IP, ETH, ETHERTYPE, SRQuerier element, LinkTable element, ARPtable element, 
   [METRIC GridGenericMetric], [WARMUP period in seconds])

=s Wifi, Wireless Routing
Sends route queries if it can't find a valid source route.

=d
 */


class SRQuerier : public Element {
 public:
  
  SRQuerier();
  ~SRQuerier();
  
  const char *class_name() const		{ return "SRQuerier"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/#"; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();

  void push(int, Packet *);
  void send_query(IPAddress);
  class DstInfo {
  public:
    DstInfo() {memset(this, 0, sizeof(*this)); }
    DstInfo(IPAddress ip) {memset(this, 0, sizeof(*this)); _ip = ip;}
    IPAddress _ip;
    int _best_metric;
    int _count;
    struct timeval _last_query;
    Path _p;
    struct timeval _last_switch;    // last time we picked a new best route
    struct timeval _first_selected; // when _p was first selected as best route
    
  };

  
  typedef HashMap<IPAddress, DstInfo> DstTable;
  DstTable _queries;

  struct timeval _query_wait;

  u_long _seq;      // Next query sequence number to use.
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint32_t _et;     // This protocol's ethertype

  IPAddress _bcast_ip;

  EtherAddress _bcast;

  class SRForwarder *_sr_forwarder;
  class LinkTable *_link_table;

  bool _route_dampening;
  bool _debug;

  int _time_before_switch_sec;

};


CLICK_ENDDECLS
#endif
