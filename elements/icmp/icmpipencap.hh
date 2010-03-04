#ifndef CLICK_ICMPIPENCAP_HH
#define CLICK_ICMPIPENCAP_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

ICMPIPEncap(SRC, DST, TYPE [, CODE, I<keywords> IDENTIFIER])

=s icmp

encapsulates packets in ICMP/IP headers

=d

Encapsulates input packets in an ICMP/IP header with source IP address SRC,
destination IP address DST, ICMP type TYPE and code CODE. TYPE and CODE
may be integers between 0 and 255 or mnemonic names; CODE defaults to 0.
Advances the "sequence" field by one for each packet. (The sequence field is
stored in network byte order in the packet.)

Keyword arguments are:

=over 8

=item CODE
Integer. Determines the ICMP code field in emitted ICMP packets. Default is 0.

=item IDENTIFIER

Integer. Determines the ICMP identifier field in emitted ICMP packets. Default is
0.

=back

=h src read/write

Returns or sets the SRC argument.

=h dst read/write

Returns or sets the DST argument.

=a

ICMPPingEncap, ICMPError */

class ICMPIPEncap : public Element { public:

    ICMPIPEncap();
    ~ICMPIPEncap();

    const char *class_name() const		{ return "ICMPIPEncap"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }
    const char *flags() const			{ return "A"; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    Packet *simple_action(Packet *);

  private:

    struct in_addr _src;
    struct in_addr _dst;
    uint16_t _icmp_id;
    uint16_t _ip_id;
    uint8_t _icmp_type;
    uint8_t _icmp_code;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    bool _aligned;
#endif

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
