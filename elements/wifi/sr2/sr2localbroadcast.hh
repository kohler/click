#ifndef CLICK_SR2LOCALBROADCAST_HH
#define CLICK_SR2LOCALBROADCAST_HH
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
#include "sr2localbroadcast.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
=c
SR2LocalBroadcast(IP, ETH, ETHERTYPE, SR2LocalBroadcast element, LinkTable element, ARPtable element, 
   [METRIC GridGenericMetric], [WARMUP period in seconds])

=s Wifi, Wireless Routing

Floods a packet to 1-hop neighbors.

 */


class SR2LocalBroadcast : public Element {
 public:
  
  SR2LocalBroadcast();
  ~SR2LocalBroadcast();
  
  const char *class_name() const		{ return "SR2LocalBroadcast"; }
  const char *port_count() const		{ return "2/2"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  static String static_print_debug(Element *f, void *);
  static int static_write_debug(const String &arg, Element *e,
				void *, ErrorHandler *errh); 

  static String static_print_stats(Element *e, void *);
  String print_stats();

  void push(int, Packet *);
  void run_timer(Timer *);

  void add_handlers();
private:

  u_long _seq;      // Next query sequence number to use.
  Timer _timer;
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype
  IPAddress _bcast_ip;

  EtherAddress _bcast;

  bool _debug;


  int _packets_originated;
  int _packets_tx;
  int _packets_rx;
};


CLICK_ENDDECLS
#endif
