#ifndef RIPSEND_HH
#define RIPSEND_HH
#include "element.hh"
#include "timer.hh"
#include "ipaddress.hh"

/*
 * =c
 * RIPSend(SRC, DST, WHAT/MASK, METRIC)
 * =d
 * Sends periodic RIP II packets with specified contents,
 * including UDP and IP headers.
 * =e
 * Send out advertisements to net 18.26.4, indicating that
 * route 18.26.4.24 knows how to get to net 10 with hop
 * count 10:
 *
 *   RIPSend(18.26.4.24, 18.26.4.255, 10.0.0.0/8, 10) ->
 *   EtherEncap(0x0008, 00:00:c0:ae:67:ef, ff:ff:ff:ff:ff:ff) ->
 *   ToDevice(eth0);
 * =n
 * Note that this is just a tiny piece of a full RIP implementation.
 */

class RIPSend : public Element {
  
  IPAddress _src; // IP header src field
  IPAddress _dst; // IP header dst field
  IPAddress _what; // Route to advertise
  IPAddress _mask;
  int _metric;

  Timer _timer;
  
 public:
  
  RIPSend();
  
  const char *class_name() const		{ return "RIPSend"; }
  const char *processing() const	{ return PUSH; }

  RIPSend *clone() const { return new RIPSend(); }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void run_scheduled();
};

#endif
