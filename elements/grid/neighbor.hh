#ifndef NEIGHBOR_HH
#define NEIGHBOR_HH

/*
 * =c
 * Neighbor(TIMEOUT, ETH, IP)
 * =d
 *
 * Neighbor is an immediate neighbor routing protocol for Grid.  It
 * encapsulates and sends packets to immediate neighbors that it knows
 * about.  When Neighbor receives a Grid packet, it remembers the
 * sender's Grid address and MAC address in its neighbor table.
 *
 * TIMEOUT is the timeout in milliseconds for entries in the neighbor
 * table.  If a negative value is specified, neighbor entries are
 * never discard.  ETH and IP are this node's MAC (Ethernet) and Grid
 * (IP) addresses, respectively.
 *
 * Neighbor expects and produces Grid packets with MAC headers on
 * input and output 0, expects IP packets annotated with a destination
 * address on input 1, and produces IP packets on output 1.
 *
 * =e This example runs the neighbor protocol for a host with Grid
 * address 13.0.0.2 listening on eth0.  Note that you need a Hello
 * element so that Grid nodes can find out about each other initially.
 *
 * = nb :: Neighbor(0, 00:E0:98:09:27:C5, 13.0.0.2)
 * = q :: Queue -> ToDevice(eth0)
 * = FromDevice(eth0) -> Classifier(12/BABE) -> [0] nb [0] -> q
 * = FromLinux(...) -> [1] nb [1] -> Queue -> ToLinux
 * = Hello(...) -> q
 *
 * =a Hello */


#include "element.hh"
#include "glue.hh"
#include "hashmap.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"

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

  HashMap<IPAddress, NbrEntry> _addresses;

  Neighbor();
  ~Neighbor();

  const char *class_name() const		{ return "Neighbor"; }
  void *cast(const char *);
  const char *processing() const		{ return AGNOSTIC; }
  Neighbor *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  
  void run_scheduled();

  void push(int port, Packet *);

  // true iff nbr is an immediate neighbor we have heard from
  bool knows_about(IPAddress nbr); 

  int _timeout_jiffies; // -1 if we are not timing out entries
private:
  IPAddress _ipaddr;
  EtherAddress _ethaddr;
};

#endif
