#ifndef CLICK_SRDESTCACHE_HH
#define CLICK_SRDESTCACHE_HH
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
#include <click/ipflowid.hh>
#include <clicknet/tcp.h>
#include "srpacket.hh"
#include "gatewayselector.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * SRDestCache([GW ipaddress], [SEL GatewaySelector element])
 * =d
 * 
 * This element marks the gateway for a packet to be sent to.
 * Either manually specifiy an gw using the GW keyword
 * or automatically select it using a GatewaySelector element.
 * 
 *
 */


class SRDestCache : public Element {
 public:
  
  SRDestCache();
  ~SRDestCache();
  
  const char *class_name() const		{ return "SRDestCache"; }
  const char *port_count() const		{ return "2/2"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/#"; }
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();
  static String read_param(Element *e, void *vparam);
  void push(int, Packet *);
private:

  class CacheEntry {
  public:
    IPAddress _client;
    IPAddress _ap;
    Timestamp _last;
    CacheEntry() {
	    _last = Timestamp::now();
    }

    CacheEntry(const CacheEntry &e) : 
	    _client(e._client),
	    _ap(e._ap),
	    _last(e._last)
	    { }
    CacheEntry(IPAddress c, IPAddress ap) {
	    CacheEntry();
	    _client = c;
	    _ap = ap;
    }

  };

  typedef HashMap<IPAddress, CacheEntry> Cache;
  typedef Cache::const_iterator FTIter;
  Cache _cache;

};


CLICK_ENDDECLS
#endif
