#ifndef LOCQUERYRESPONDER_HH
#define LOCQUERYRESPONDER_HH

/*
 * =c
 * LocQueryResponder(E, I)
 * =s Grid
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
CLICK_DECLS

class LocQueryResponder : public Element {

 public:
  LocQueryResponder() CLICK_COLD;
  ~LocQueryResponder() CLICK_COLD;

  const char *class_name() const		{ return "LocQueryResponder"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);


private:
  IPAddress _ip;
  EtherAddress _eth;
  Timer _expire_timer;

  static const int EXPIRE_TIMEOUT_MS = 2 * 1000;
  unsigned int _timeout_jiffies;
  static void expire_hook(Timer *, void *);

  struct seq_t {
    unsigned int seq_no;
    unsigned int last_jiffies;
    seq_t(unsigned int s, int j) : seq_no(s), last_jiffies(j) { }
    seq_t() : seq_no(0), last_jiffies(0) { }
  };

  typedef HashMap<IPAddress, seq_t> seq_map;
  seq_map _query_seqs;

};

CLICK_ENDDECLS
#endif
