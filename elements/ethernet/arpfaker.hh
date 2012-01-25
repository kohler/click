#ifndef CLICK_ARPFAKER_HH
#define CLICK_ARPFAKER_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * ARPFaker(DSTIP, DSTETH, SRCIP, SRCETH)
 * =s arp
 * periodically generates an ARP reply
 * =d
 * Every 10 seconds,
 * sends an ARP "reply" packet to DSTIP/DSTETH claiming that SRCIP has ethernet
 * address SRCETH. Generates the ethernet header as well as the
 * ARP header.
 *
 * =e
 * Sends ARP packets to 18.26.4.1 (with ether addr 00-E0-2B-0B-1A-00)
 * claiming that 18.26.4.99's ethernet address is 00-A0-C9-9C-FD-9C.
 *
 *   ARPFaker(18.26.4.1, 0:e0:2b:b:1a:0, 18.26.4.99, 00:a0:c9:9c:fd:9c)
 *      -> ToDevice(eth0);
 *
 * =n
 * You probably want to use ARPResponder rather than ARPFaker.
 *
 * =a
 * ARPQuerier, ARPResponder
 */

class ARPFaker : public Element {

public:

  ARPFaker();
  ~ARPFaker();

  const char *class_name() const		{ return "ARPFaker"; }
  const char *port_count() const		{ return PORTS_0_1; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void run_timer(Timer *);

private:

  IPAddress _ip1;
  EtherAddress _eth1;
  IPAddress _ip2;
  EtherAddress _eth2;

  Timer _timer;

};

CLICK_ENDDECLS
#endif
