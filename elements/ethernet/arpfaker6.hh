#ifndef ArpFaker6_HH
#define ArpFaker6_HH

/*
 * =c
 * ARPFaker6(IP1, ETH1, IP2, ETH2)
 * =s
 * V<ARP>
 * =d
 * Every 10 seconds,
 * sends an ARP "reply" packet to IP1/ETH1 claiming that IP2 has ethernet
 * address ETH2. Generates the ethernet header as well as the
 * ARP header.
 * 
 * =e
 * Sends ARP packets to 18.26.4.1 (with ether addr 0:e0:2b:b:1a:0)
 * claiming that 18.26.4.99's ethernet address is 00:a0:c9:9c:fd:9c.
 *
 *   ARPFaker(18.26.4.1, 0:e0:2b:b:1a:0, 18.26.4.99, 00:a0:c9:9c:fd:9c)
 *       -> ToDevice(eth0);
 * 
 * =n
 * You probably want to use ARPResponder rather than ARPFaker.
 *
 * =a
 * ARPQuerier6, ARPResponder6
 */

#include "element.hh"
#include "timer.hh"
#include "etheraddress.hh"
#include "ip6address.hh"
#include "hashmap.hh"

class ARPFaker6 : public Element {
public:
  ARPFaker6();
  ~ARPFaker6();
  
  const char *class_name() const		{ return "ARPFaker6"; }
  const char *processing() const		{ return PUSH; }
  ARPFaker6 *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  Packet *make_response(unsigned char tha[6], unsigned char tpa[16],
                        unsigned char sha[6], unsigned char spa[16]);
  
  void run_scheduled();
  
private:
  
  IP6Address _ip1;
  EtherAddress _eth1;
  IP6Address _ip2;
  EtherAddress _eth2;
  
  Timer _timer;
};

#endif
