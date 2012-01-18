#ifndef LOOKUPIPROUTEMP_HH
#define LOOKUPIPROUTEMP_HH
#include <click/element.hh>
#include <click/iptable.hh>
CLICK_DECLS

/*
 * =c
 * LookupIPRouteMP(DST1/MASK1 [GW1] OUT1, DST2/MASK2 [GW2] OUT2, ...)
 * =s threads
 * simple static IP routing table
 * V<classification>
 * =d
 * Interfaces are exactly the same as LookupIPRoute. The only difference is
 * when operating in SMP mode, each processor has a processor local cache,
 * hence the element is MT SAFE.
 *
 * =a LookupIPRoute
 */

class LookupIPRouteMP : public Element {
#ifdef CLICK_LINUXMODULE
  static const int _cache_buckets = NR_CPUS;
#else
  static const int _cache_buckets = 1;
#endif

  struct cache_entry {
    IPAddress _last_addr_1;
    IPAddress _last_gw_1;
    int _last_output_1;
    IPAddress _last_addr_2;
    IPAddress _last_gw_2;
    int _last_output_2;
    int pad[2];
  };

  // XXX a bit annoying that we don't get better alignment =(
  int _pad[2];
  struct cache_entry _cache[_cache_buckets];

  IPTable _t;

public:
  LookupIPRouteMP();
  ~LookupIPRouteMP();

  const char *class_name() const		{ return "LookupIPRouteMP"; }
  const char *port_count() const		{ return "1/-"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int port, Packet *p);
};

CLICK_ENDDECLS
#endif
