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
#include <elements/wifi/arptable.hh>
#include <elements/wifi/sr/path.hh>
#include "srquerier.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * SRQuerier(IP, ETH, ETHERTYPE, SRQuerier element, LinkTable element, ARPtable element, 
 *    [METRIC GridGenericMetric], [WARMUP period in seconds])
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet data packets from device 
 * Input 1: IP packets from higher layer, w/ ip addr anno.
 * Input 2: data packets
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ethernet packets to device (data)
 *
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

  class DstInfo {
  public:
    DstInfo() {memset(this, 0, sizeof(*this)); }
    DstInfo(IPAddress ip) {memset(this, 0, sizeof(*this)); _ip = ip;}
    IPAddress _ip;
    u_long _seq;
    int _best_metric;
    int _count;
    struct timeval _last_query;

  };

  
  class CurrentPath {
  public:
    Path _p;
    struct timeval _last_switch;    // last time we picked a new best route
    struct timeval _first_selected; // when _p was first selected as best route
    CurrentPath() { }
    CurrentPath(Path p) { _p = p; }
  };

  typedef HashMap<IPAddress, CurrentPath> PathCache;
  PathCache _path_cache;

  typedef HashMap<IPAddress, DstInfo> DstInfoTable;
  DstInfoTable _queries;

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

  void start_query(IPAddress);
};


CLICK_ENDDECLS
#endif
