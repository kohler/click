/* -*- c-basic-offset: 2 -*- */
#ifndef CLICK_ICMPSENDPINGS_HH
#define CLICK_ICMPSENDPINGS_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
=c

ICMPSendPings(SADDR, DADDR, I<KEYWORDS>)

=s ICMP, sources

periodically sends ICMP echo requests

=d

Periodically emits ping packets with source IP address SRC and destination
address DST. Advances the "sequence" field by one each time. (The sequence
field is stored in network byte order in the packet.)

Keyword arguments are:

=over 8

=item INTERVAL

Amount of time between pings, in seconds. Default is 1.

=item IDENTIFIER

Integer. Determines the ICMP identifier field in emitted pings. Default is
0.

=item LIMIT

Integer. The number of pings to send; but if LIMIT is negative, sends pings
forever. Default is -1.

=item DATA

String. Extra data in emitted pings. Default is the empty string (nothing).

=back

=a

ICMPPingResponder, ICMPPingRewriter */

class ICMPSendPings : public Element { public:
  
  ICMPSendPings();
  ~ICMPSendPings();
  
  const char *class_name() const		{ return "ICMPSendPings"; }
  const char *processing() const		{ return PUSH; }
  
  ICMPSendPings *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void run_timer();
  
 private:
  
  struct in_addr _src;
  struct in_addr _dst;
  int _count;
  int _limit;
  uint16_t _icmp_id;
  int _interval;
  Timer _timer;
  String _data;
  
};

CLICK_ENDDECLS
#endif
