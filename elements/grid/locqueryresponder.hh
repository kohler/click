#ifndef LOCQUERYRESPONDER_HH
#define LOCQUERYRESPONDER_HH

/*
 * =c
 * LocQueryResponder(E, I)
 * =s
 * generates responses to Grid Location queries
 * =d
 * 
 * E and I are this node's ethernet and IP addresses, respectively.
 * Input should be Grid location query packets destined for us,
 * including the MAC header.  Produces a GRID_LOC_REPLY packet with
 * the correct destination IP and location information.  This packet
 * should probably be sent back through a routing element,
 * e.g. through LookupLocalGridRoute's MAC layer input, and then
 * through FixSrcLoc (to actually get this node's location information
 * into the packet).
 *
 * =a FloodingLocQuerier, LookupLocalGridRoute,
 * LookupGeographicGridRoute, FixSrcLoc */

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
#include <click/timer.hh>

class LocQueryResponder : public Element {

 public:
  LocQueryResponder();
  ~LocQueryResponder();
  
  const char *class_name() const		{ return "LocQueryResponder"; }
  const char *processing() const		{ return AGNOSTIC; }
  LocQueryResponder *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
  

private:
  IPAddress _ip;
  EtherAddress _eth;
  Timer _expire_timer;

  static const int EXPIRE_TIMEOUT_MS = 2 * 1000;
  int _timeout_jiffies;
  static void expire_hook(unsigned long);

  struct seq_t {
    unsigned int seq_no;
    int last_jiffies;
    seq_t(unsigned int s, int j) : seq_no(s), last_jiffies(j) { }
    seq_t() : seq_no(0), last_jiffies(0) { }
  };

  typedef BigHashMap<IPAddress, seq_t> seq_map;
  seq_map _query_seqs;

};

#endif
