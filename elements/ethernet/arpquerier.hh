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

ARPQuerier will not send queries for packets addressed to 0.0.0.0,
255.255.255.255, or, if specified, any BROADCAST address.  Packets addressed
to 0.0.0.0 are dropped; packets for broadcast addresses are forwarded with
destination Ethernet address FF:FF:FF:FF:FF:FF.

Keyword arguments are:

=over 8

=item TABLE

Element.  Names an ARPTable element that holds this element's corresponding
ARP state.  By default ARPQuerier creates its own internal ARPTable and uses
that.  If TABLE is specified, CAPACITY, ENTRY_CAPACITY, and TIMEOUT are
ignored.

=item CAPACITY

Unsigned integer.  The maximum number of saved IP packets the table will
hold at a time.  Default is 2048.

=item ENTRY_CAPACITY

Unsigned integer.  The maximum number of ARP entries the table will hold
at a time.  Default is 0, which means unlimited.

=item TIMEOUT

Time in seconds.  Amount of time before an ARP entry expires.  Defaults to
1 minute.

=item BROADCAST

IP address.  Local broadcast IP address.  Packets sent to this address will be
forwarded to Ethernet address FF:FF:FF:FF:FF:FF.  Defaults to the local
broadcast address that can be extracted from the IP address's corresponding
prefix, if any.

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
their next packet annotations.

ARPQuerier will send at most 10 queries a second for any IP address.

=h ipaddr read/write

Returns or sets the ARPQuerier's source IP address.

=h broadcast read-only

Returns the ARPQuerier's IP broadcast address.

=h table read-only

Returns a textual representation of the ARP table.  See ARPTable's table
handler.

=h stats read-only

Returns textual statistics (queries and drops).

=h queries read-only

Returns the number of queries sent.

=h responses read-only

Returns the number of responses received.

=h drops read-only

Returns the number of packets dropped.

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

    ARPQuerier();
    ~ARPQuerier();

    const char *class_name() const		{ return "ARPQuerier"; }
    const char *port_count() const		{ return "2/1-2"; }
    const char *processing() const		{ return PUSH; }
    const char *flow_code() const		{ return "xy/x"; }
    void *cast(const char *name);

    int configure(Vector<String> &, ErrorHandler *);
    int live_reconfigure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }
    int initialize(ErrorHandler *errh);
    void add_handlers();
    void cleanup(CleanupStage stage);
    void take_state(Element *e, ErrorHandler *errh);

    void push(int port, Packet *p);

  private:

    ARPTable *_arpt;
    EtherAddress _my_en;
    IPAddress _my_ip;
    IPAddress _my_bcast_ip;

    // statistics
    atomic_uint32_t _arp_queries;
    atomic_uint32_t _drops;
    atomic_uint32_t _arp_responses;
    atomic_uint32_t _broadcasts;
    bool _my_arpt;

    void send_query_for(Packet *p);

    void handle_ip(Packet *p, bool response);
    void handle_response(Packet *p);

    static void expire_hook(Timer *, void *);
    static String read_table(Element *, void *);
    static String read_table_xml(Element *, void *);
    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

    enum { h_table, h_table_xml, h_stats, h_insert, h_delete, h_clear };

};

CLICK_ENDDECLS
#endif
