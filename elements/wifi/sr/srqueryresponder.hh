#ifndef CLICK_SRQUERYRESPONDER_HH
#define CLICK_SRQUERYRESPONDER_HH
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
#include "srqueryresponder.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * SRQueryResponder(IP, ETH, ETHERTYPE, SRQueryResponder element, LinkTable element, ARPtable element, 
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


class SRQueryResponder : public Element {
 public:
  
  SRQueryResponder();
  ~SRQueryResponder();
  
  const char *class_name() const		{ return "SRQueryResponder"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/#"; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();

  void push(int, Packet *);

  int get_fwd_metric(IPAddress other);
  int get_rev_metric(IPAddress other);
  bool update_link(IPAddress from, IPAddress to, uint32_t seq, int metric);

  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint32_t _et;     // This protocol's ethertype

  class SRForwarder *_sr_forwarder;
  class LinkTable *_link_table;
  class LinkMetric *_metric;
  class ARPTable *_arp_table;

  bool _debug;



  int find_dst(IPAddress ip, bool create);
  EtherAddress find_arp(IPAddress ip);
  void got_arp(IPAddress ip, EtherAddress en);

  void start_reply(struct srpacket *pk);
  void forward_reply(struct srpacket *pk);
  void got_reply(struct srpacket *pk);

  void send(WritablePacket *);
};


CLICK_ENDDECLS
#endif
