#ifndef IP6FRAGMENTER_HH
#define IP6FRAGMENTER_HH

/*
 * =c
 * IP6Fragmenter(MTU)
 * =s IPv6
 * 
 * =d
 * Expects IP6 packets as input.
 * If the IP6 packet size is <= mtu, just emits the packet on output 0.
 * If the size is greater than mtu and DF isn't set, splits into
 * fragments emitted on output 0.
 * If DF is set and size is greater than mtu, sends to output 1.
 * 
 * Ordinarily output 1 is connected to an ICMP6Error packet generator
 * with type 3 (UNREACH) and code 4 (NEEDFRAG).
 *
 * Only the mac_broadcast annotation is copied into the fragments.
 *
 * Sends the first fragment last.
 *
 * =e
 * Example:
 *
 *   ... -> fr::IP6Fragmenter -> Queue(20) -> ...
 *   fr[1] -> ICMP6Error(18.26.4.24, 3, 4) -> ...
 *
 * =a ICMP6Error, CheckLength
 */

#include <click/element.hh>
#include <click/glue.hh>

class IP6Fragmenter : public Element {

  unsigned _mtu;
  int _drops;
  int _fragments;
  
  void fragment(Packet *);
  //int optcopy(const click_ip6 *ip1, click_ip6 *ip2);

 public:
  
  IP6Fragmenter();
  ~IP6Fragmenter();
  
  const char *class_name() const		{ return "IP6Fragmenter"; }
   const char *processing() const	{ return PUSH; }
  void notify_noutputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  
  int drops() const				{ return _drops; }
  int fragments() const				{ return _fragments; }
  
  IP6Fragmenter *clone() const;
  void add_handlers();

  void push(int, Packet *p);
  
  
};

#endif
