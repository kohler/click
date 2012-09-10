#ifndef CLICK_IPENCAP_HH
#define CLICK_IPENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/ip.h>
CLICK_DECLS

/*
=c

IPEncap(PROTO, SRC, DST, I<KEYWORDS>)

=s ip

encapsulates packets in static IP header

=d

Encapsulates each incoming packet in an IP packet with protocol
PROTO, source address SRC, and destination address DST.
This is most useful for IP-in-IP encapsulation.
Its destination address annotation is also set to DST.

As a special case, if DST is "DST_ANNO", then the destination address
is set to the incoming packet's destination address annotation.

Keyword arguments are:

=over 8

=item TTL

Byte. The IP header's time-to-live field. Default is 250.

=item DSCP

Number between 0 and 63. The IP header's DSCP value. Default is 0.

=item ECT

Boolean or "2". If true, sets the IP header's ECN bits to ECN Capable
Transport. If "true", "1" or "yes", sets the ECN bits to 1; but if "2", sets
them to 2. Default is false.

=item CE

Boolean. If true, sets the IP header's ECN bits to 3 (Congestion Experienced).
Default is false.

=item TOS

Byte. The IP header's TOS value. Default is 0. If you specify TOS, you may not
specify DSCP, ECT, or CE.

=item DF

Boolean. If true, sets the IP header's Don't Fragment bit to 1. Default is
false.

=back

The StripIPHeader element can be used by the receiver to get rid
of the encapsulation header.

=e

Wraps packets in an IP header specifying IP protocol 4
(IP-in-IP), with source 18.26.4.24 and destination 140.247.60.147:

  IPEncap(4, 18.26.4.24, 140.247.60.147)

You could also say "C<IPEncap(ipip, ...)>".

=h src read/write

Returns or sets the SRC parameter.

=h dst read/write

Returns or sets the DST parameter.

=a UDPIPEncap, StripIPHeader */

class IPEncap : public Element { public:

  IPEncap() CLICK_COLD;
  ~IPEncap() CLICK_COLD;

  const char *class_name() const		{ return "IPEncap"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }
  int initialize(ErrorHandler *) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);

 private:

  click_ip _iph;
  atomic_uint32_t _id;

  inline void update_cksum(click_ip *, int) const;
  static String read_handler(Element *, void *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
