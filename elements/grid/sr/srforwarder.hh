#ifndef CLICK_SRForwarder_HH
#define CLICK_SRForwarder_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <elements/grid/linktable.hh>
#include <click/vector.hh>
#include <elements/grid/sr/path.hh>
CLICK_DECLS

/*
 * =c
 * SRForwarder(ETHERTYPE, IP, ETH, ARPTable element, LT LinkTable element
 *     [ETT element], [METRIC GridGenericMetric] )
 * =d
 * DSR-inspired ad-hoc routing protocol.
 * Input 0: Incoming ethernet packets for me
 * Output 0: Outgoing ethernet packets
 * Output 1: IP packets for higher layer
 *
 * Normally usged in conjuction with ETT element
 *
 */


class SRForwarder : public Element {
 public:
  
  SRForwarder();
  ~SRForwarder();
  
  const char *class_name() const		{ return "SRForwarder"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  SRForwarder *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);

  /* handler stuff */
  void add_handlers();

  static String static_print_stats(Element *e, void *);
  String print_stats();
  static String static_print_routes(Element *e, void *);
  String print_routes();

  void push(int, Packet *);
  
  void send(const u_char *payload, u_long len, Vector<IPAddress>, int flags);
  Packet *encap(const u_char *payload, u_long len, Vector<IPAddress>, int flags);
private:

  IPAddress _ip;    // My IP address.
  EtherAddress _eth; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype
  // Statistics for handlers.
  int _datas;
  int _databytes;

  EtherAddress _bcast;

  class LinkTable *_link_table;
  class ARPTable *_arp_table;
  class SRCR *_srcr;
  class SrcrStat *_srcr_stat;
  
  int get_metric(IPAddress other);

  void update_link(IPAddress from, IPAddress to, int metric);
  void srforwarder_assert_(const char *, int, const char *) const;
};


CLICK_ENDDECLS
#endif
