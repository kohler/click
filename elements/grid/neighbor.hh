#ifndef NEIGHBOR_HH
#define NEIGHBOR_HH


#include "element.hh"
#include "glue.hh"
#include "hashmap.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"

class Neighbor : public Element {

public:

#if 0
  class eth_ip_pair {
    bool _init;
  public:
    EtherAddress eth;
    IPAddress ip;
    eth_ip_pair() : _init(false) { }
    eth_ip_pair(unsigned char *eth_in, unsigned char *ip_in) : _init(true), eth(eth_in), ip(ip_in) { }
    operator bool() const { return _init; }
    unsigned int hashcode() const { return *(unsigned int *)ip.data(); }
    String s() const { return eth.s() + " -- " + ip.s(); }
  };
#endif

  HashMap<IPAddress, EtherAddress> _addresses;

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

private:
  IPAddress _ipaddr;
  EtherAddress _ethaddr;
};

#endif
