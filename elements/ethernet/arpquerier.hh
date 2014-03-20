#ifndef CLICK_ARPQUERIER_HH
#define CLICK_ARPQUERIER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include "arptable.hh"
CLICK_DECLS

/*
=c

ARPQuerier(IP, ETH, I<keywords>)
ARPQuerier(NAME, I<keywords>)

=s arp

encapsulates IP packets in Ethernet headers found via ARP

=d

Handles most of the ARP protocol. Argument IP should be
this host's IP address, and ETH should be this host's
Ethernet address. (In
the one-argument form, NAME should be shorthand for
both an IP and an Ethernet address; see AddressInfo(n).)

Packets arriving on input 0 should be IP packets, and must have their
destination address annotations set.
If an Ethernet address is already known
for the destination, the IP packet is wrapped in an Ethernet
header and sent to output 0. Otherwise the IP packet is saved and
an ARP query is sent instead. If an ARP response arrives
on input 1 for an IP address that we need, the mapping is
recorded and any saved IP packets are sent.

The ARP reply packets on input 1 should include the Ethernet header.

ARPQuerier may have one or two outputs. If it has two, then ARP queries
are sent to the second output.

ARPQuerier implements special behavior for 0.0.0.0, 255.255.255.255, multicast
addresses, and, if specified, any BROADCAST address.  Packets addressed to
0.0.0.0 are dropped.  Packets for broadcast addresses are forwarded with
destination Ethernet address FF-FF-FF-FF-FF-FF.  Multicast IP addresses are
forwarded to 01-00-5E-xx-yy-zz, where xx-yy-zz are the lower 23 bits of the
multicast IP address, as specified in RFC1112.

Keyword arguments are:

=over 8

=item TABLE

Element.  Names an ARPTable element that holds this element's corresponding
ARP state.  By default ARPQuerier creates its own internal ARPTable and uses
that.  If TABLE is specified, CAPACITY, ENTRY_CAPACITY, ENTRY_PACKET_CAPACITY,
and TIMEOUT are ignored.

=item CAPACITY

Unsigned integer.  The maximum number of saved IP packets the table will
hold at a time.  Default is 2048.

=item ENTRY_CAPACITY

Unsigned integer.  The maximum number of ARP entries the table will hold
at a time.  Default is 0, which means unlimited.

=item ENTRY_PACKET_CAPACITY

Unsigned integer.  The maximum number of saved packets the table will hold
for any given ARP entry at a time.  Default is 0, which means unlimited.

=item TIMEOUT

Amount of time before an ARP entry expires.  Defaults to 5 minutes.

=item POLL_TIMEOUT

Amount of time after which ARPQuerier will start polling for renewal.  0 means
don't poll.  Defaults to one minute.

=item BROADCAST

IP address.  Local broadcast IP address.  Packets sent to this address will be
forwarded to Ethernet address FF-FF-FF-FF-FF-FF.  Defaults to the local
broadcast address that can be extracted from the IP address's corresponding
prefix, if any.

=item BROADCAST_POLL

Boolean.  If true, then send broadcast ARP polls (where an entry is about to
expire, but hasn't expired yet).  The default is to send such polls unicast to
the known Ethernet address.  Defaults to false.

=back

=e

   c :: Classifier(12/0806 20/0002, 12/0800, ...);
   a :: ARPQuerier(18.26.4.24, 00:00:C0:AE:67:EF);
   c[0] -> a[1];
   c[1] -> ... -> a[0];
   a[0] -> ... -> ToDevice(eth0);

=n

If a host has multiple interfaces, it will need multiple
instances of ARPQuerier.

ARPQuerier uses packets' destination IP address annotations, and can destroy
their next packet annotations.  Generated ARP queries have VLAN TCI
annotations set from the corresponding input packets.

ARPQuerier will send at most 10 queries a second for any IP address.

=h ipaddr rw

Returns or sets the ARPQuerier's source IP address.

=h broadcast r

Returns the ARPQuerier's IP broadcast address.

=h table r

Returns a textual representation of the ARP table.  See ARPTable's table
handler.

=h stats r

Returns textual statistics (queries and drops).

=h queries r

Returns the number of queries sent.

=h responses r

Returns the number of responses received.

=h drops r

Returns the number of packets dropped.

=h count r

Returns the number of entries in the ARP table.

=h length r

Returns the number of packets stored in the ARP table.

=h insert w

Add an entry to the ARP table.  The input string should have the form "IP ETH".

=h delete w

Delete an entry from the ARP table.  The input string should be an IP address.

=h clear w

Clear the ARP table.

=a

ARPTable, ARPResponder, ARPFaker, AddressInfo
*/

class ARPQuerier : public Element { public:

    ARPQuerier() CLICK_COLD;
    ~ARPQuerier() CLICK_COLD;

    const char *class_name() const		{ return "ARPQuerier"; }
    const char *port_count() const		{ return "2/1-2"; }
    const char *processing() const		{ return PUSH; }
    const char *flow_code() const		{ return "xy/x"; }
    // click-undead should consider all paths live (not just "xy/x"):
    const char *flags() const			{ return "L2"; }
    void *cast(const char *name);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int live_reconfigure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    void cleanup(CleanupStage stage) CLICK_COLD;
    void take_state(Element *e, ErrorHandler *errh);

    void push(int port, Packet *p);

  private:

    ARPTable *_arpt;
    EtherAddress _my_en;
    IPAddress _my_ip;
    IPAddress _my_bcast_ip;
    uint32_t _poll_timeout_j;
    int _broadcast_poll;

    // statistics
    atomic_uint32_t _arp_queries;
    atomic_uint32_t _drops;
    atomic_uint32_t _arp_responses;
    atomic_uint32_t _broadcasts;
    bool _my_arpt;
    bool _zero_warned;

    void send_query_for(const Packet *p, bool ether_dhost_valid);

    void handle_ip(Packet *p, bool response);
    void handle_response(Packet *p);

    static void expire_hook(Timer *, void *);
    static String read_table(Element *, void *);
    static String read_table_xml(Element *, void *);
    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

    enum { h_table, h_table_xml, h_stats, h_insert, h_delete, h_clear,
	   h_count, h_length };

};

CLICK_ENDDECLS
#endif
