#ifndef CLICK_IPMIRROR_HH
#define CLICK_IPMIRROR_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

IPMirror([DST_ANNO])

=s ip

swaps IP source and destination

=d

Incoming packets must have their IP header annotations set. Swaps packets'
source and destination IP addresses. Packets containing TCP or UDP
headers---that is, first fragments of packets with protocol 6 or 17---also
have their source and destination ports swapped. TCP packets also have their
seq and ack numbers swapped.

The IP and TCP or UDP checksums are not changed. They don't need to be; these
swap operations do not affect checksums.

By default the output packet's destination address annotation is set to the
new destination address.  Pass "false" for the DST_ANNO argument to leave the
annotation as is.  DST_ANNO defaults to true.

*/

class IPMirror : public Element { public:

    IPMirror() CLICK_COLD;
    ~IPMirror() CLICK_COLD;

    const char *class_name() const		{ return "IPMirror"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    Packet *simple_action(Packet *);

  private:

    bool _dst_anno;

};

CLICK_ENDDECLS
#endif // CLICK_IPMIRROR_HH
