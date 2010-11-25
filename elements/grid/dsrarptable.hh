#ifndef DSRARPTABLE_HH
#define DSRARPTABLE_HH

#include <click/element.hh>
#include "dsrroutetable.hh"

#include <click/bighashmap.hh>
CLICK_DECLS


/*
 * =c
 * DSRArpTable(IP, ETHER)
 *
 * =s Grid
 * Maintains an ARP table for DSR.
 *
 * =d
 *
 * Packets with ethernet headers pushed into input 2 are sent out
 * unchanged on output 2.  IP<->MAC address entries are added to the
 * ARP table for each pushed packet's source MAC address and source IP
 * address (derived from the various DSR option headers).
 *
 * Pulls on outputs 0 and 1 pull packets from inputs 0 and 1,
 * respectively.  The pulled packets have a link-level MAC header
 * added to them and are sent out output 0.  The destination MAC
 * address is found by using the packet's destination IP annotation to
 * lookup the MAC in the ARP table.
 *
 * Design rant follows (by Doug):
 *
 * Why two pull inputs that do exactly the same thing and go to their
 * respective separate outputs?  I don't know, but I conjecture it's
 * to allow this element to be used on the output of two separate
 * queues.  This is a bad design (I was going to write suboptimal, but
 * it's BAD).  The element should be split into two: DSRArpTable
 * (which is agnostic and records IP<->MAC) mappings from packets
 * flowing through it), and DSRLookupArp (which is also agnostic,
 * takes DSRArpTable element as an argument, and lookups and writes
 * MAC destination addresses for any packet's passing through it based
 * on the entries in DSRArpTable).  The advantage of this design is
 * that you can have as many DSRLookupArps as you want, in either push
 * or pull paths.  Even better, split DSRArpTable into DSRArpTable,
 * which never even handles packets, and DSRSnoopARPEntry, which takes
 * DSRArpTable as an argument: packets passing through
 * DSRSnoopARPEntry have their IP<->MAC mappings added to DSRArpTable.
 * Then you can have as many DSRSnoop elements as you like.  Actually,
 * the Snoop and ArpTable elements might even be completely generic,
 * and could be shared with regular IP/Ethernet ARP configurations.
 * Of course, this may be all wrong if you want to do
 * buffering/timeouts of packets in DSRLookupARP...
 *
 * Regular arguments are:
 *
 * =over 8
 * =item IP
 *
 * This node's IP address.
 *
 * =item ETHER
 *
 * This node's ethernet address.
 *
 * =back
 * =a
 * DSRRouteTable */

/*
  todo: timeout entries.  the pull/push thing is retarded, but i
  expect this element to be folded into routetable anyway.  see
  also the todo list in dsrroutetable.hh.
*/

/*
 * that is a nice idea, but I don't have time for this now and want
 * to build a router configuration that uses notifying queues _after_
 * the dsrarptable to make it Click not hog the CPU. Thus I made
 * the first two ports agnostic.
 */

class DSRArpTable : public Element
{

public:

  DSRArpTable();
  ~DSRArpTable();

  const char *class_name() const	{ return "DSRArpTable"; }
  const char *port_count() const	{ return "3/3"; }
  const char *processing() const	{ return "aah/aah"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);

  Packet *pull(int);
  void push(int, Packet *);

private:

  unsigned int _etht;

  IPAddress _me;
  EtherAddress _me_ether;

  typedef HashMap<IPAddress, EtherAddress> IPMap;
  IPMap _ip_map;

  IPAddress last_hop_ip(Packet *);

  void add_entry(IPAddress, EtherAddress);
  void delete_entry(IPAddress);
  EtherAddress lookup_ip(IPAddress);
  bool _debug;
  Packet* lookup_arp(Packet *);
};
CLICK_ENDDECLS
#endif

