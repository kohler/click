#ifndef LOOKUPIPROUTEMP_HH
#define LOOKUPIPROUTEMP_HH

/*
 * =c
 * LookupIPRouteMP(DST1/MASK1 [GW1] OUT1, DST2/MASK2 [GW2] OUT2, ...)
 * =s
 * simple static IP routing table
 * V<classification>
 * =d
 * Interfaces are exactly the same as LookupIPRoute. The only difference is
 * when operating in SMP mode, each processor has a processor local cache,
 * hence the element is MT SAFE.
 *
 * =a LookupIPRoute
 */

#include <click/element.hh>
#include <click/iptable.hh>

class LookupIPRouteMP : public Element {
#ifdef __KERNEL__
  static const int _cache_buckets = NR_CPUS;
#else
  static const int _cache_buckets = 1;
#endif
 
  // XXX a bit annoying that we don't get better alignment =(
  int _pad[2];
  struct {
    IPAddress _last_addr_1;
    IPAddress _last_gw_1;
    int _last_output_1;
    IPAddress _last_addr_2;
    IPAddress _last_gw_2;
    int _last_output_2;
    int pad[2];
  } _cache[_cache_buckets];

  IPTable _t;

public:
  LookupIPRouteMP();
  ~LookupIPRouteMP();
  
  const char *class_name() const		{ return "LookupIPRouteMP"; }
  const char *processing() const		{ return AGNOSTIC; }
  LookupIPRouteMP *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int port, Packet *p);
};

#endif
