#ifndef GRIDROUTETABLE_HH
#define GRIDROUTETABLE_HH

/*
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/bighashmap.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include "grid.hh"
#include <click/timer.hh>

class GridRouteTable : public Element {

public:

  GridRouteTable();
  ~GridRouteTable();

  const char *class_name() const		{ return "GridRouteTable"; }
  void *cast(const char *);
  const char *processing() const		{ return "h/h"; }
  const char *flow_code() const                 { return "x/y"; }
  GridRouteTable *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  
  void get_rtes(Vector<grid_nbr_entry> *retval);

  class RTEntry {
    bool _init;

  public:
    IPAddress dest_ip; // IP address of this destination
    IPAddress next_hop_ip; // IP address of next hop for this destination
    EtherAddress next_hop_eth; // hardware address of next hop
    unsigned char num_hops; // number of hops to dest

    struct grid_location loc; // location of dest, as contained in its route ads
    unsigned short loc_err;
    bool loc_good;

    unsigned int seq_no; 
    unsigned int ttl;  // msecs

    int last_updated_jiffies; // last time this entry was updated

    RTEntry() : 
      _init(false), dest_ip(0), next_hop_eth(0), num_hops(0), loc_good(false),
      seq_no(0), age(0), last_updated_jiffies(-1) { }
    
    RTEntry(IPAddress _dest_ip, IPAddress _next_hop_ip, EtherAddress _next_hop_eth,
	    unsigned char _num_hops, grid_location _loc, unsigned short _loc_err, 
	    bool _loc_good, unsigned int _seq_no, unsigned int _ttl, 
	    int _last_updated_jiffies) :
      _init(true), dest_ip(_dest_ip), next_hop_ip(_next_hop_ip), 
      next_hop_eth(_next_hop_eth), num_hops(_num_hops), loc(_loc), 
      loc_err(_loc_err), loc_good(_loc_good), seq_no(_seq_no), ttl(_ttl),
      last_updated_jiffies(_last_updated_jiffies) { }

    /* constructor for 1-hop route entry */
    RTEntry(IPAddress ip, EtherAddress eth, grid_header *gh, grid_hello *hlo,
	    unsigned int jiff) :
      _init(true), dest_ip(ip), next_hop_ip(ip), next_hopt_eth(eth), num_hops(1), 
      loc(gh->loc), loc_err(ntohs(gh->loc_err)), loc_good(gh->loc_good), 
      seq_no(ntohl(hlo->seq_no)), 
      ttl(decr_ttl(ntohl(hlo->age), grid_hello::MIN_TTL_DECREMENT)),
      last_updated_jiffies(jiff) { }

    /* constructor from grid_nbr_entry */
    RTEntry(IPAddress ip, EtherAddress eth, grid_nbr_entry *nbr, 
	    unsigned int jiff) :
      _init(true), dest_ip(nbr->ip), next_hop_ip(ip), next_hop_eth(eth),
      num_hops(nbr->num_hops + 1), loc(nbr->loc), loc_err(nbr->loc_err),
      loc_good(nbr->loc_good), seq_no(ntohl(nbr->seq_no)), 
      ttl(decr_ttl(ntohl(nbr->age), grid_hello::MIN_TTL_DECREMENT)),
      last_updated_jiffies(jiff) { }

  };

  typedef BigHashMap<IPAddress, RTEntry> RTable;
  typedef RTable::Iterator RTIter;

  RTable _rtes;

private:
  /* max time to keep an entry in RT */
  int _timeout; // msecs, -1 if we are not timing out entries
  int _timeout_jiffies;

  /* route broadcast timing parameters */
  int _period;
  int _jitter;

  /* interval at which to check RT entries for expiry */
  static const unsigned int EXPIRE_TIMER_PERIOD = 100; // msecs


  /* this node's addresses */
  IPAddress _ip;
  EtherAddress _eth;

  /* latest sequence number for this node's route entry */
  unsigned int _seq_no;

  /* local DSDV radius */
  int _max_hops;


  Timer _expire_timer;
  Timer _hello_timer;

  static void expire_hook(Timer *, void *);
  Vector<RTEntry> expire_routes();
  
  static void hello_hook(Timer *, void *);
  
  void send_routing_update(Vector<RTEntry> &rte_info, bool);

  static unsigned int decr_ttl(unsigned int ttl, unsigned int decr)
  { return (ttl > decr ? ttl - decr : 0); }
};

#endif





