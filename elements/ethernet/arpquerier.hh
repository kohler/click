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

ARPQuerier(I, E)
ARPQuerier(NAME)

=s Ethernet, encapsulation

encapsulates IP packets in Ethernet headers found via ARP

=d

Handles most of the ARP protocol. Argument I should be
this host's IP address, and E should be this host's
Ethernet address.
(In the one-argument form, NAME should be shorthand for
both an IP and an Ethernet address; see AddressInfo(n).)

Packets arriving on input 0 should be IP packets, and must have their
destination address annotations set.
If an Ethernet address is already known
for the destination, the IP packet is wrapped in an Ethernet
header and sent to output 0. Otherwise the IP packet is saved and
an ARP query is sent instead. If an ARP response arrives
on input 1 for an IP address that we need, the mapping is
recorded and the saved IP packet is sent.

The ARP reply packets on input 1 should include the Ethernet header.

If a host has multiple interfaces, it will need multiple
instances of ARPQuerier.

ARPQuerier may have one or two outputs. If it has two, then ARP queries
are sent to the second output.

=e

   c :: Classifier(12/0806 20/0001, 12/0800, ...);
   a :: ARPQuerier(18.26.4.24, 00:00:C0:AE:67:EF);
   c[0] -> a[1];
   c[1] -> ... -> a[0];
   a[0] -> ... -> ToDevice(eth0);

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
  
  ARPQuerier *clone() const;
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

  struct ARPEntry {
    IPAddress ip;
    EtherAddress en;
    int last_response_jiffies;
    unsigned ok: 1;
    unsigned polling: 1;
    Packet *p;
    struct ARPEntry *next;
  };

  // statistics
  uatomic32_t _arp_queries;
  uatomic32_t _pkts_killed;
  
 private:
  ReadWriteLock _lock;

  enum { NMAP = 256 };
  ARPEntry *_map[NMAP];
  EtherAddress _my_en;
  IPAddress _my_ip;
  Timer _expire_timer;
  
  void send_query_for(const IPAddress &);
  
  void handle_ip(Packet *);
  void handle_response(Packet *);

  enum { EXPIRE_TIMEOUT_MS = 60 * 1000 };
  static void expire_hook(Timer *, void *);
  static String read_table(Element *, void *);
  
};

CLICK_ENDDECLS
#endif
