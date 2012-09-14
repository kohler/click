#ifndef CLICK_RIPSEND_HH
#define CLICK_RIPSEND_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * RIPSend(SRC, DST, PREFIX, METRIC)
 * =s iproute
 * periodically generates specified RIP II packet
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

  RIPSend() CLICK_COLD;
  ~RIPSend() CLICK_COLD;

  const char *class_name() const		{ return "RIPSend"; }
  const char *port_count() const		{ return PORTS_0_1; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void run_timer(Timer *);

};

CLICK_ENDDECLS
#endif
