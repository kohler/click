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
 * Output 0: Outgoing ethernet packets that I forward
 * Output 1: outgoing ethernet packets originating from me
 * Output 2: IP packets for higher layer
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
  
  void send(Packet *, Vector<IPAddress>, int flags);
  Packet *encap(Packet *, Vector<IPAddress>, int flags);
  IPAddress ip() { return _ip; }
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
  
  class PathInfo {
  public:
    Path _p;
    int _seq;
    struct timeval _last_tx;
    void reset() { _seq = 0; }
    PathInfo() :  _p() { reset(); }
    PathInfo(Path p) :  _p(p)  { reset(); }
  };
  typedef BigHashMap<Path, PathInfo> PathTable;
  PathTable _paths;
  
  int get_metric(IPAddress other);

  void update_link(IPAddress from, IPAddress to, int metric);
  void srforwarder_assert_(const char *, int, const char *) const;
};


CLICK_ENDDECLS
#endif
