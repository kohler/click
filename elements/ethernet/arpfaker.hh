#ifndef ArpFaker_HH
#define ArpFaker_HH

/*
 * =c
 * ARPFaker(IP1, ETH1, IP2, ETH2)
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
 * = ARPFaker(18.26.4.1, 0:e0:2b:b:1a:0, 18.26.4.99, 00:a0:c9:9c:fd:9c) ->
 * = ToBPF(eth0);
 * 
 * =n
 * You probably want to use ARPResponder rather than ARPFaker.
 *
 * =a ARPQuerier
 * =a ARPResponder
 */

#include "timedelement.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "hashmap.hh"

class ARPFaker : public TimedElement {
public:
  ARPFaker();
  ~ARPFaker();
  
  const char *class_name() const		{ return "ARPFaker"; }
  Processing default_processing() const	{ return PUSH; }
  ARPFaker *clone() const;
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  Packet *make_response(unsigned char tha[6], unsigned char tpa[4],
                        unsigned char sha[6], unsigned char spa[4]);
  
  void run_scheduled();
  
private:
  
  IPAddress _ip1;
  EtherAddress _eth1;
  IPAddress _ip2;
  EtherAddress _eth2;
  
};

#endif
