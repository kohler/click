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

#include "element.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "vector.hh"
#include "bighashmap.hh"

class LocQueryResponder : public Element {

 public:
  LocQueryResponder();
  ~LocQueryResponder();
  
  const char *class_name() const		{ return "LocQueryResponder"; }
  const char *processing() const		{ return AGNOSTIC; }
  LocQueryResponder *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  

private:
  IPAddress _ip;
  EtherAddress _eth;

  typedef BigHashMap<IPAddress, unsigned int> seq_map;
  seq_map _query_seqs;

};

#endif
