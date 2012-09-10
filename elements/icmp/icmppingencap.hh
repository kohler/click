// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ICMPPINGENCAP_HH
#define CLICK_ICMPPINGENCAP_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

ICMPPingEncap(SRC, DST [, I<keyword> IDENTIFIER])

=s icmp

encapsulates packets in ICMP ping headers

=d

Encapsulates input packets in an ICMP ping header with source IP address SRC
and destination IP address DST.  Advances the "sequence" field by one for
each packet.  (The sequence field is stored in network byte order in the
packet.)

Keyword arguments are:

=over 8

=item IDENTIFIER

Integer. Determines the ICMP identifier field in emitted pings. Default is
0.

=back

=h src read/write

Returns or sets the SRC argument.

=h dst read/write

Returns or sets the DST argument.

=a

ICMPPingSource, ICMPPingResponder, ICMPPingRewriter */

class ICMPPingEncap : public Element { public:

    ICMPPingEncap() CLICK_COLD;
    ~ICMPPingEncap() CLICK_COLD;

    const char *class_name() const		{ return "ICMPPingEncap"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *flags() const			{ return "A"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    struct in_addr _src;
    struct in_addr _dst;
    uint16_t _icmp_id;
    uint16_t _ip_id;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    bool _aligned;
#endif

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
