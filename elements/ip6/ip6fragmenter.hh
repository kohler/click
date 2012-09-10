#ifndef CLICK_IP6FRAGMENTER_HH
#define CLICK_IP6FRAGMENTER_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * IP6Fragmenter(MTU)
 * =s ip6
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
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PUSH; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  int drops() const				{ return _drops; }
  int fragments() const				{ return _fragments; }

  void add_handlers() CLICK_COLD;

  void push(int, Packet *p);


};

CLICK_ENDDECLS
#endif
