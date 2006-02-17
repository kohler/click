// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ICMPPINGENCAP_HH
#define CLICK_ICMPPINGENCAP_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

ICMPPingEncap(SADDR, DADDR [, I<keyword> IDENTIFIER])

=s icmp

encapsulates packets in ICMP ping headers

=d

Encapsulates input packets in an ICMP ping header with source IP address SADDR
and destination IP address DADDR.  Advances the "sequence" field by one for
each packet.  (The sequence field is stored in network byte order in the
packet.)

Keyword arguments are:

=over 8

=item IDENTIFIER

Integer. Determines the ICMP identifier field in emitted pings. Default is
0.

=back

=a

ICMPPingSource, ICMPPingResponder, ICMPPingRewriter */

class ICMPPingEncap : public Element { public:

    ICMPPingEncap();
    ~ICMPPingEncap();

    const char *class_name() const		{ return "ICMPPingEncap"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }
    
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
