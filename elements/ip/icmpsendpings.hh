#ifndef ICMPSENDPINGS_HH
#define ICMPSENDPINGS_HH
#include "element.hh"
#include "timer.hh"
#include "ipaddress.hh"

/*
 * =c
 * ICMPSendPings(SADDR, DADDR)
 * =s
 * periodically sends ICMP echo requests
 * =d
 * Send one ping packet per second. SADDR and DADDR are IP addresses.
 */

class ICMPSendPings : public Element {
  
  struct in_addr _src;
  struct in_addr _dst;
  Timer _timer;
  int _id;
  
 public:
  
  ICMPSendPings();
  
  const char *class_name() const		{ return "ICMPSendPings"; }
  const char *processing() const		{ return PUSH; }
  
  ICMPSendPings *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();
  
};

#endif
