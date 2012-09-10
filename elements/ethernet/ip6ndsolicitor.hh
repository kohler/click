#ifndef CLICK_IP6NDSOLICITOR_HH
#define CLICK_IP6NDSOLICITOR_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ip6address.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * IP6NDSolicitor(IP, ETH)
 * =s ip6
 *
 * =d
 * Handles most of the Neighbor Discovery(ND) protocol.
 * Argument IP should be this host's IP6 address, and ETH should
 * be this host's ethernet address.
 *
 * Expects ordinary IP6 packets on input 0, each with a destination
 * address annotation. If an ethernet address is already known
 * for the destination, the IP6 packet is wrapped in an ethernet
 * header and sent to output 0. Otherwise the IP6 packet is saved and
 * an Neighbor Solicitation Message is sent to output 0.
 * If an Neighbor Advertisement Message arrives
 * on input 1 for an IP6 address that we need, the mapping is
 * recorded and the saved IP6 packet is sent.
 *
 * The packets on input 1 should include the ethernet header.
 *
 * If a host has multiple interfaces, it will need multiple
 * instances of IP6NDSolicitor.
 *
 * IP6NDSolicitor may have one or two outputs. If it has two, then ARP queries
 * are sent to the second output.
 *
 * =e
 *    c :: Classifier(12/86dd 20/3aff 54/87,
 *		      12/86dd 20/3aff 54/88,
 *		      12/86dd);
 *    nds :: IP6NDSolicitor(3ffe:1ce1:2::1, 00:e0:29:05:e5:6f);
 *    c[0] -> ...
 *    c[1] -> nds[1];
 *    c[2] -> ... -> nds[0];
 *    nds[0] -> ... -> ToDevice(eth0);
 *
 * =a
 * IP6NDAdvertiser
 */

class IP6NDSolicitor : public Element {
 public:

  IP6NDSolicitor();
  ~IP6NDSolicitor();

  const char *class_name() const		{ return "IP6NDSolicitor"; }
  const char *port_count() const		{ return "2/1-2"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "xy/x"; }

  void add_handlers() CLICK_COLD;

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  void take_state(Element *, ErrorHandler *);

  void push(int port, Packet *);

  Packet *make_query(unsigned char tpa[16],
                     unsigned char sha[6], unsigned char spa[16]);

  void insert(IP6Address, EtherAddress);

   struct NDEntry {
    IP6Address ip6;
    EtherAddress en;
    click_jiffies_t last_response_jiffies;
    unsigned ok: 1;
    unsigned polling: 1;
    Packet *p;
    struct NDEntry *next;
  };

  // statistics
  int _arp_queries;
  int _pkts_killed;

 private:

  enum { NMAP = 256 };
  NDEntry *_map[NMAP];
  EtherAddress _my_en;
  IP6Address _my_ip6;
  Timer _expire_timer;

  void send_query_for(const u_char want_ip6[16]);

  void handle_ip6(Packet *);
  void handle_response(Packet *);

  enum { EXPIRE_TIMEOUT_MS = 15 * 1000 };
  static void expire_hook(Timer *, void *);
  static String read_table(Element *, void *);

};

CLICK_ENDDECLS
#endif
