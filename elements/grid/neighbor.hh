#ifndef NEIGHBOR_HH
#define NEIGHBOR_HH

/*
 * =c
 * Neighbor(TIMEOUT, PERIOD, JITTER, ETH, IP [, MAX-HOPS])
 * =d
 *
 * Maintain local routing table, with entry timeout of TIMEOUT
 * millseconds.
 *
 * Every PERIOD millseconds (+/- a jitter bounded by JITTER
 * milliseconds), emit a Grid protocol ``LR_HELLO'' packet for the
 * Grid node at address IP with MAC address ETH, advertising any
 * neighbors within MAX-HOPS of the node, as reported by the Neighbor
 * element named by the 5th argument.  MAX-HOPS defaults to 1.  PERIOD
 * must be greater than 0, JITTER must be positive and less than
 * JITTER.  Produces Ethernet packets.
 *
 * =a
 * Hello, FixSrcLoc, SetGridChecksum, LocalRoute 
 */


#include "element.hh"
#include "glue.hh"
#include "bighashmap.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "grid.hh"
#include "timer.hh"

class Neighbor : public Element {

public:

  class NbrEntry {
    bool _init;
  public:
    EtherAddress eth;
    IPAddress ip;
    int last_updated_jiffies;
    NbrEntry() : _init(false), last_updated_jiffies(-1) { }
    NbrEntry(EtherAddress eth_in, IPAddress ip_in, int jiff) 
      : _init(true), eth(eth_in), ip(ip_in), last_updated_jiffies(jiff) { }
    operator bool() const { return _init; }
    unsigned int hashcode() const { return *(unsigned int *)ip.data(); }
    String s() const 
    { return eth.s() + " -- " + ip.s() + " -- " + String(last_updated_jiffies); }
  };
  typedef BigHashMap<IPAddress, NbrEntry> Table;
  Table _addresses; // immediate nbrs
  /* 
   * _addresses is a mapping from IP to ether for nodes within our
   * radio range.  this information is extraced by snooping on all
   * packets with grid headers .  
   */
 
  struct far_entry {
    far_entry() : last_updated_jiffies(0) { }
    far_entry(int j, grid_nbr_entry n) : last_updated_jiffies(j), nbr(n) { }
    int last_updated_jiffies;
    grid_nbr_entry nbr;
  };
  typedef BigHashMap<IPAddress, far_entry> FarTable;
  FarTable _nbrs; // immediate and multihop nbrs
  /* 
   * _nbrs is our routing table; its information is maintained by
   * processing Grid Hello packets only.  one invariant: any entry
   * listed as one hop in _nbrs has an entry in _addresses. 
   */

  Neighbor();
  ~Neighbor();

  const char *class_name() const		{ return "Neighbor"; }
  void *cast(const char *);
  const char *processing() const		{ return AGNOSTIC; }
  Neighbor *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  
  Packet *simple_action(Packet *);

  bool get_next_hop(IPAddress dest_ip, EtherAddress *dest_eth) const;
  void get_nbrs(Vector<grid_nbr_entry> *retval) const;

  Packet *make_hello();
  
private:
  int _timeout; // -1 if we are not timing out entries
  int _timeout_jiffies;

  String get_nbrs();

  IPAddress _ipaddr;
  EtherAddress _ethaddr;
  int _max_hops;

  int _period;
  int _jitter;
  Timer _hello_timer;
  Timer _expire_timer;
  unsigned int _seq_no;

  static void expire_hook(unsigned long);
  static void hello_hook(unsigned long);
  void expire_routes();
};

#endif





