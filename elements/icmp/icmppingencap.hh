// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ICMPPINGENCAP_HH
#define CLICK_ICMPPINGENCAP_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

ICMPPingEncap(SADDR, DADDR, I<KEYWORDS>)

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

class ICMPPingEncap : public Element { public:

    ICMPPingEncap();
    ~ICMPPingEncap();

    const char *class_name() const		{ return "ICMPPingEncap"; }
    const char *processing() const		{ return AGNOSTIC; }
    ICMPPingEncap *clone() const;
    
    int configure(Vector<String> &, ErrorHandler *);

    Packet *simple_action(Packet *);

  private:
  
    struct in_addr _src;
    struct in_addr _dst;
    uint16_t _icmp_id;
    uint16_t _ip_id;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    bool _aligned;
#endif

};

CLICK_ENDDECLS
#endif
