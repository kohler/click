#ifndef ARPQUERIER6_HH
#define ARPQUERIER6_HH

/*
 * =c
 * ARPQuerier6(I, E)
 * =s
 * V<ARP, encapsulation>
 * =d
 * Handles most of the ARP protocol. Argument I should be
 * this host's IP6 address, and E should be this host's
 * ethernet address.
 *
 * Expects ordinary IP6 packets on input 0, each with a destination
 * address annotation. If an ethernet address is already known
 * for the destination, the IP6 packet is wrapped in an ethernet
 * header and sent to output 0. Otherwise the IP6 packet is saved and
 * an ARP query is sent to output 0. If an ARP response arrives
 * on input 1 for an IP6 address that we need, the mapping is
 * recorded and the saved IP packet is sent.
 *
 * The ARP reply packets on input 1 should include the ethernet header.
 *
 * If a host has multiple interfaces, it will need multiple
 * instances of ARPQuerier.
 *
 * =e
 *    c :: Classifier(12/0806 20/0001, 12/0800, ...);
 *    a :: ARPQuerier6(0::121A:0459, 00:00:C0:AE:67:EF);
 *    c[0] -> a[1];
 *    c[1] -> ... -> a[0];
 *    a[0] -> ... -> ToDevice(eth0);
 *
 * =a
 * ARPResponder6, ARPFaker6
 */

#include "element.hh"
#include "etheraddress.hh"
#include "ip6address.hh"
#include "timer.hh"

class ARPQuerier6 : public Element {
 public:
  
  ARPQuerier6();
  ~ARPQuerier6();
  
  const char *class_name() const		{ return "ARPQuerier6"; }
  const char *processing() const		{ return PUSH; }
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  void notify_noutputs(int);
  void add_handlers();
  
  ARPQuerier6 *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void take_state(Element *, ErrorHandler *);
  
  void push(int port, Packet *);
  
  Packet *make_query(unsigned char tpa[16],
                     unsigned char sha[6], unsigned char spa[16]);

  void insert(IP6Address, EtherAddress);

   struct ARPEntry6 {
    IP6Address ip6;
    EtherAddress en;
    int last_response_jiffies;
    unsigned ok: 1;
    unsigned polling: 1;
    Packet *p;
    struct ARPEntry6 *next;
  };

  // statistics
  int _arp_queries;
  int _pkts_killed;
  
 private:

  static const int NMAP = 256;
  ARPEntry6 *_map[NMAP];
  EtherAddress _my_en;
  IP6Address _my_ip6;
  Timer _expire_timer;
  
  void send_query_for(const u_char want_ip6[16]);
  
  void handle_ip6(Packet *);
  void handle_response(Packet *);

  static const int EXPIRE_TIMEOUT_MS = 15 * 1000;
  static void expire_hook(unsigned long);
  static String read_table(Element *, void *);
  
};

#endif

