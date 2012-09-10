#ifndef CLICK_IP6ENCAP_HH
#define CLICK_IP6ENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/ip6.h>
#include <click/ip6address.hh>

CLICK_DECLS

/*
=c

IP6Encap(PROTO, SRC, DST, I<KEYWORDS>)

=s ip

encapsulates packets in static IP6 header

=d

Encapsulates each incoming packet in an IP6 packet with next header proto
PROTO, source address SRC, and destination address DST.
Its destination address annotation is also set to DST.

As a special case, if DST is "DST_ANNO", then the destination address
is set to the incoming packet's destination address annotation.

Keyword arguments are:

=over 2

=item HLIM

Integer, The hop limit of the packet with maximum value of 255

=item CLASS

Integer, The service class of the packet.  Used for QoS

=e
Wraps packets in an IP6 header specifying IP protocol 4
(IP6-in-IP4), with source 2000:10:1::2 and destination 2000:20:1::2,
HLIM is set to 20 hops:

  IP6Encap(4, 2000:10:1::2, 2000:20:1::2, HLIM 20)

You could also say "C<IP6Encap(ipip, ...)>".

=h src read/write

Returns or sets the SRC parameter.

=h dst read/write

Returns or sets the DST parameter.

=a UDPIP6Encap */

class IP6Encap : public Element { public:

  IP6Encap();
  ~IP6Encap();

  const char *class_name() const        { return "IP6Encap"; }
  const char *port_count() const        { return PORTS_1_1; }

  int  configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const     { return true; }
  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);

 private:

  click_ip6 _iph6;
  bool      _use_dst_anno;

  static String read_handler(Element *, void *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
