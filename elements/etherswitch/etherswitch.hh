#ifndef CLICK_ETHERSWITCH_HH
#define CLICK_ETHERSWITCH_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/hashtable.hh>
CLICK_DECLS

/*
=c

EtherSwitch([I<keywords> TIMEOUT])

=s ethernet

learning, forwarding Ethernet switch

=d

Expects and produces Ethernet packets.  Each pair of corresponding ports
(e.g., input 0 and output 0, and input 1 and output 1, and so forth)
corresponds to a LAN.  Acts as a learning, forwarding Ethernet switch among
those LANs.

On receiving a packet on input port I with source address A, EtherSwitch
associates A with I.  Future packets destined for A are sent to output I
(unless they came from input I, in which case they are dropped).  Packets sent
to an unknown destination address are forwarded to every output port, except
the one corresponding to the packet's input port.  The TIMEOUT parameter
affects how long port associations last.  If it is 0, then the element does
not learn addresses, and acts like a dumb hub.

Keyword arguments are:

=over 8

=item TIMEOUT

The timeout for port associations, in seconds.  Any port mapping (i.e.,
binding between an address and a port number) is dropped after TIMEOUT seconds
of inactivity.  If 0, the element acts like a dumb hub.  Default is 300.

=back

=n

The EtherSwitch element has no limit on the memory consumed by cached Ethernet
addresses.

=h table read-only

Returns the current port association table.

=h timeout read/write

Returns or sets the TIMEOUT argument.

=a

ListenEtherSwitch, EtherSpanTree
*/

class EtherSwitch : public Element { public:

  EtherSwitch() CLICK_COLD;
  ~EtherSwitch() CLICK_COLD;

  const char *class_name() const		{ return "EtherSwitch"; }
  const char *port_count() const		{ return "2-/="; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/[^#]"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;

  void push(int port, Packet* p);

    struct AddrInfo {
	int port;
	Timestamp stamp;
	inline AddrInfo(int p, const Timestamp &t);
    };

  private:

    typedef HashTable<EtherAddress, AddrInfo> Table;
    Table _table;
    uint32_t _timeout;

    void broadcast(int source, Packet*);

    static String reader(Element *, void *);
    static int writer(const String &, Element *, void *, ErrorHandler *);
    friend class ListenEtherSwitch;

};

inline
EtherSwitch::AddrInfo::AddrInfo(int p, const Timestamp& s)
    : port(p), stamp(s)
{
}

CLICK_ENDDECLS
#endif
