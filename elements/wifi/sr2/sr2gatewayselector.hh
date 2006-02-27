#ifndef CLICK_SR2GatewaySelector_HH
#define CLICK_SR2GatewaySelector_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/wifi/linktable.hh>
#include <elements/wifi/arptable.hh>
#include <elements/wifi/path.hh>
CLICK_DECLS

/*
=c
SR2GatewaySelector(IP, ETH, ETHERTYPE, LinkTable, ARPTable, 
                [PERIOD timeout], [GW is_gateway], 
                [METRIC GridGenericMetric])

=s Wifi, Wireless Routing

Select a gateway to send a packet to based on TCP connection
state and metric to gateway.

=d

Input 0: packets from dev
Input 1: packets for gateway node
Output 0: packets to dev
Output 1: packets with dst_ip anno set

This element provides proactive gateway selection.  
Each gateway broadcasts an ad every PERIOD seconds.  
Non-gateway nodes select the gateway with the best metric
and forward ads.

 */


class SR2GatewaySelector : public Element {
 public:
  
  SR2GatewaySelector();
  ~SR2GatewaySelector();
  
  const char *class_name() const		{ return "SR2GatewaySelector"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();

  String print_gateway_stats();

  static int write_param(const String &arg, Element *e,
				void *, ErrorHandler *errh); 
  
  static String read_param(Element *, void *);

  static int static_pick_new_gateway(const String &arg, Element *el,
				     void *, ErrorHandler *errh);
  void push(int, Packet *);
  void run_timer(Timer *);

  bool update_link(IPAddress from, IPAddress to, uint32_t seq, uint32_t metric);
  void forward_ad_hook();
  IPAddress best_gateway();
  bool is_gateway() { return _is_gw; }
private:
    // List of query sequence #s that we've already seen.
  class Seen {
  public:
    IPAddress _gw;
    u_long _seq;
    int _count;
    Timestamp _when; /* when we saw the first query */
    Timestamp _to_send;
    bool _forwarded;
    Seen(IPAddress gw, u_long seq, int fwd, int rev) {
	(void) fwd, (void) rev;
	_gw = gw; 
	_seq = seq; 
	_count = 0;
    }
    Seen();
  };
  
  DEQueue<Seen> _seen;



  typedef HashMap<IPAddress, IPAddress> IPTable;
  typedef IPTable::const_iterator IPIter;
  IPTable _ignore;
  IPTable _allow;
  
  class GWInfo {
  public:
    IPAddress _ip;
    Timestamp _first_update;
    Timestamp _last_update;
    int _seen;
    GWInfo() {memset(this,0,sizeof(*this)); }
  };

  typedef HashMap<IPAddress, GWInfo> GWTable;
  typedef GWTable::const_iterator GWIter;
  GWTable _gateways;


  int MaxSeen;   // Max size of table of already-seen queries.
  int MaxHops;   // Max hop count for queries.
  Timestamp _gw_expire;
  u_long _seq;      // Next query sequence number to use.
  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype
  unsigned int _period; // msecs

  EtherAddress _bcast;
  bool _is_gw;

  class LinkTable *_link_table;
  class ARPTable *_arp_table;
  Timer _timer;





  int find_dst(IPAddress ip, bool create);
  EtherAddress find_arp(IPAddress ip);
  void got_arp(IPAddress ip, EtherAddress en);
  void got_sr_pkt(Packet *p_in);
  void start_ad();
  void process_query(struct sr_pkt *pk);
  void forward_ad(Seen *s);
  void send(WritablePacket *);
  void process_data(Packet *p_in);
  bool pick_new_gateway();
  bool valid_gateway(IPAddress);
  static void static_forward_ad_hook(Timer *, void *e) { 
    ((SR2GatewaySelector *) e)->forward_ad_hook(); 
  }
};


CLICK_ENDDECLS
#endif
