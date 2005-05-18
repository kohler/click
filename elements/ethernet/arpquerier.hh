#ifndef CLICK_ARPQUERIER_HH
#define CLICK_ARPQUERIER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/sync.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

ARPQuerier(I, E, I<keywords>)
ARPQuerier(NAME, I<keywords>)

=s Ethernet, encapsulation

encapsulates IP packets in Ethernet headers found via ARP

=d

Handles most of the ARP protocol. Argument I should be
this host's IP address, and E should be this host's
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

Keyword arguments are:

=over 8

=item CAPACITY

Unsigned integer.  The maximum number of saved IP packets the element will
hold at a time.  Default is 2048.  Note that, unlike the number of packets,
the total number of ARP entries the element will hold is currently unlimited.

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

=h table read-only

Returns a textual representation of the ARP table.

=h stats read-only

Returns textual statistics (queries and drops).

=h queries read-only

Returns the number of queries sent.

=h responses read-only

Returns the number of responses received.

=h drops read-only

Returns the number of packets dropped.

=a

ARPResponder, ARPFaker, AddressInfo
*/

class ARPQuerier : public Element { public:
  
  ARPQuerier();
  ~ARPQuerier();
  
  const char *class_name() const		{ return "ARPQuerier"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "xy/x"; }

  void notify_noutputs(int);
  void add_handlers();
  
  int configure(Vector<String> &, ErrorHandler *);
  int live_reconfigure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void clear_map();
  
  void take_state(Element *, ErrorHandler *);
  
  void push(int port, Packet *);
  
  Packet *make_query(unsigned char tpa[4],
                     unsigned char sha[6], unsigned char spa[4]);

    struct ARPEntry {		// This structure is now larger than I'd like
	IPAddress ip;		// (40B) but probably still fine.
	EtherAddress en;	// Deleting head and pprev could get it down to
	unsigned ok: 1;		// 32B, with some time cost.
	unsigned polling: 1;	// It used to be 24B... :|
	int last_response_jiffies;
	Packet *head;
	Packet *tail;
	ARPEntry *next;
	ARPEntry **pprev;
	ARPEntry *age_next;
	ARPEntry **age_pprev;
    };

  private:
    
    ReadWriteLock _lock;

    enum { NMAP = 256 };
    ARPEntry *_map[NMAP];
    ARPEntry *_age_head;
    ARPEntry *_age_tail;
    EtherAddress _my_en;
    IPAddress _my_ip;
    Timer _expire_timer;
    uint32_t _capacity;
  
    // statistics
    atomic_uint32_t _cache_size;
    atomic_uint32_t _arp_queries;
    atomic_uint32_t _drops;
    atomic_uint32_t _arp_responses;
    
    static inline int ip_bucket(IPAddress);
    void send_query_for(IPAddress);
  
    void handle_ip(Packet *);
    void handle_response(Packet *);

    enum { EXPIRE_TIMEOUT_MS = 60 * 1000 };
    static void expire_hook(Timer *, void *);
    static String read_table(Element *, void *);
    static String read_stats(Element *, void *);
  
};

inline int
ARPQuerier::ip_bucket(IPAddress ipa)
{
    return (ipa.data()[0] + ipa.data()[3]) % NMAP;
}

CLICK_ENDDECLS
#endif
