#ifndef ARPQUERIER_HH
#define ARPQUERIER_HH

/*
 * =c
 * ARPQuerier(I, E)
 * =d
 * Handles most of the ARP protocol. Argument I should be
 * this host's IP address, and E should be this host's
 * ethernet address.
 *
 * Expects ordinary IP packets on input 0, each with a destination
 * address annotation. If an Ethernet address is already known
 * for the destination, the IP packet is wrapped in an Ethernet
 * header and sent to output 0. Otherwise the IP packet is saved and
 * an ARP query is sent instead. If an ARP response arrives
 * on input 1 for an IP address that we need, the mapping is
 * recorded and the saved IP packet is sent.
 *
 * The ARP reply packets on input 1 should include the Ethernet header.
 *
 * If a host has multiple interfaces, it will need multiple
 * instances of ARPQuerier.
 *
 * ARPQuerier may have one or two outputs. If it has two, then ARP queries
 * are sent to the second output.
 *
 * =e
 * = c :: Classifier(12/0806 20/0001, 12/0800, ...);
 * = a :: ARPQuerier(18.26.4.24, 00:00:C0:AE:67:EF);
 * = c[0] -> a[1];
 * = c[1] -> ... -> a[0];
 * = a[0] -> ... -> ToDevice(eth0);
 *
 * =a ARPResponder
 * =a ARPFaker
 */

#include "element.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "timer.hh"

class ARPQuerier : public Element {
 public:
  
  ARPQuerier();
  ~ARPQuerier();
  
  const char *class_name() const		{ return "ARPQuerier"; }
  Processing default_processing() const		{ return PUSH; }
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  void notify_noutputs(int);
  void add_handlers(HandlerRegistry *fcr);
  
  ARPQuerier *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
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
  int _arp_queries;
  int _pkts_killed;
  
 private:

  static const int NMAP = 256;
  ARPEntry *_map[NMAP];
  EtherAddress _my_en;
  IPAddress _my_ip;
  Timer _expire_timer;
  
  void send_query_for(const IPAddress &);
  
  void handle_ip(Packet *);
  void handle_response(Packet *);

  static const int EXPIRE_TIMEOUT_MS = 15 * 1000;
  static void expire_hook(unsigned long);
  static String read_table(Element *, void *);
  
};

#endif
