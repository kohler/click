#ifndef DSRARPTABLE_HH
#define DSRARPTABLE_HH

#include <click/element.hh>
#include "dsrroutetable.hh"

#include <click/bighashmap.hh>


/*
 * =c
 * DSRArpTable(IP, ETHER)
 *
 * =s Grid
 * Maintains an ARP table for DSR.
 *
 * =d 
 * 
 * Packets with ethernet headers are expected on input 1, and are
 * sent out on output 1 unchanged.  IP<->MAC address entries are
 * added for the source MAC address and the source IP address
 * (derived from the various DSR option headers).
 * 
 * Packets on input 0 have a link-level header added to them and are
 * sent out output 0.  The destination MAC is based on the
 * destination IP annotation.
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

class DSRArpTable : public Element
{      

public:
  
  DSRArpTable();
  ~DSRArpTable();
  
  const char *class_name() const	{ return "DSRArpTable"; }
  const char *processing() const	{ return "lh/lh"; }
  
  DSRArpTable *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh); 

  Packet *pull(int);
  void push(int, Packet *);

private:

  IPAddress _me;
  EtherAddress _me_ether;

  typedef HashMap<IPAddress, EtherAddress> IPMap;
  IPMap _ip_map;

  IPAddress last_hop_ip(Packet *);

  void add_entry(IPAddress, EtherAddress);
  void delete_entry(IPAddress);
  EtherAddress lookup_ip(IPAddress);

};
#endif





































