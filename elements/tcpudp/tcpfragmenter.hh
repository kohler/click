#ifndef CLICK_TCPFRAGMENTER_HH
#define CLICK_TCPFRAGMENTER_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TCPFragmenter(MTU)

=s tcp

fragments TCP packets to a maximum TCP payload size

=d

TCP Packets with payload length greater than the MTU are fragmented into
multiple packets each containing at most MTU bytes of TCP payload.  Each of
these new packets will be a copy of the input packet except for checksums (ip
and tcp), length (ip length), and tcp sequence number (for all fragments except
the first).  This means that TCPFragmenter can operate on packets that have
ethernet headers, and all ethernet headers will be copied to each fragment.

=a IPFragmenter, TCPIPEncap
*/

class TCPFragmenter : public Element { public:

    TCPFragmenter() CLICK_COLD;
    ~TCPFragmenter() CLICK_COLD;

    const char *class_name() const	{ return "TCPFragmenter"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PUSH; }
    bool can_live_reconfigure() const	{ return true; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    void push(int, Packet *);

  private:
    uint16_t _mtu;
};

CLICK_ENDDECLS
#endif
