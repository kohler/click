#ifndef CLICK_TOP5PATHTRACKER_HH
#define CLICK_TOP5PATHTRACKER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/grid/linktable.hh>
#include <elements/grid/arptable.hh>
#include <elements/grid/sr/path.hh>
#include "srcr.hh"
#include "path.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * Top5PathTracker(IP, ETH, ETHERTYPE, [METRIC GridGenericMetric])
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet packets 
 * Input 1: ethernet data packets destined to device
 * Input 2: IP packets from higher layer, w/ ip addr anno.
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ethernet packets to device (data)
 *
 */


class Top5PathTracker : public Element {
 public:
  
  Top5PathTracker();
  ~Top5PathTracker();
  
  const char *class_name() const		{ return "Top5PathTracker"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  Top5PathTracker *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();

  static String static_print_stats(Element *e, void *);
  String print_stats();

  void push(int, Packet *);

private:

  class Src {
  public:
    IPAddress _ip;
    Vector<Path> _paths;
    Vector<int> _count_per_path;
    uint32_t _seq;
    int _current_path;
    int _packets_sent_on_current_path;
    bool _started;
    bool _finished;
    Vector<struct timeval> _first_received;
    int _best_path;
    Src() : _paths(), _count_per_path(), _first_received() { }
  };

  typedef HashMap<IPAddress, Src> SrcTable;
  SrcTable _sources;


  struct timeval _time_per_path;
  u_long _seq;      // Next query sequence number to use.
  uint32_t _et;     // This protocol's ethertype
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.

  class SRForwarder *_sr_forwarder;
  class LinkTable *_link_table;
  class SrcrStat *_srcr_stat;
  class ARPTable *_arp_table;

  int find_dst(IPAddress ip, bool create);
  EtherAddress find_arp(IPAddress ip);
  void send(WritablePacket *);
  void top5pathtracker_assert_(const char *, int, const char *) const;

};


CLICK_ENDDECLS
#endif
