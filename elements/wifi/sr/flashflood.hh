#ifndef CLICK_FLASHFLOOD_HH
#define CLICK_FLASHFLOOD_HH
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
#include <elements/wifi/sr/path.hh>
#include "flashflood.hh"
//#include "ettmetric.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * FlashFlood(ETHTYPE eth, IP ip, BCAST_IP ip, ETH eth, COUNT int, 
 *              MAX_DELAY int, 
 *              [DEBUG bool], [HISTORY int]);
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet packets from dev
 * Input 1: ip packets from higher layer 
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ip packets to higher layer
 *
 * =over 8
 *
 * =item COUNT
 * 
 * count of x indicates don't forward if you've recieved x packets
 * Count of 0 indicates always forward
 * Count of 1 indicates never forward; like a local broadcast
 * 
 * 
 * =item MAX_DELAY
 *
 * max time to wait after 1st packet rx to forward packet. default is 750
 *
 * =item HISTORY
 *
 * number of sequence numbers to remember. default is 100
 *
 *
 *
 *
 */


class ETTMetric;

class FlashFlood : public Element {
 public:
  
  FlashFlood();
  ~FlashFlood();
  
  const char *class_name() const		{ return "FlashFlood"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  static String static_print_debug(Element *f, void *);
  static String static_print_min_p(Element *e, void *);

  static int write_param(const String &arg, Element *e,
				void *, ErrorHandler *errh); 

  void clear();

  static String read_param(Element *e, void *);
  String print_packets();

  void push(int, Packet *);

  void add_handlers();
private:

  typedef HashMap<IPAddress, int> IPProbMap;


  class SeqProbMap {
  public:
    uint32_t _seq;
    IPProbMap _node_to_prob;
    Vector<IPAddress> _senders;
    Vector<uint32_t> _link_seq;
  };



  class Broadcast {
  public:
    uint32_t _seq;
    bool _originated; /* this node started the bcast */
    Packet *_p;
    int _num_rx;
    int _num_tx;
    struct timeval _first_rx;
    bool _actual_first_rx;
    bool _scheduled;
    bool _sent;
    Timer *t;
    struct timeval _to_send;
    
    Vector<IPAddress> _rx_from; /* who I got this packet from */
    Vector<uint32_t> _rx_from_seq;
    Vector<uint32_t> _sent_seq;

    void del_timer() {
      if (t) {
	t->unschedule();
	delete t;
	t = NULL;
      }
    }

  };


  DEQueue<SeqProbMap> _mappings;
  DEQueue<Broadcast> _packets;

  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint32_t _et;     // This protocol's ethertype
  IPAddress _bcast_ip;

  EtherAddress _bcast;


  LinkTable *_link_table;

  bool _debug;
  bool _lossy;
  bool _pick_slots;
  bool _slots_nweight;
  bool _slots_erx;
  bool _process_own_sends;

  int _packets_originated;
  int _packets_tx;
  int _packets_rx;

  int _history;
  int _min_p;

  int _threshold;
  int _neighbor_threshold;
  int _slot_time_ms;
  void forward(Broadcast *bcast);
  void forward_hook();
  void trim_packets();
  int get_link_prob(IPAddress from, IPAddress to);
  void update_probs(uint32_t seq, uint32_t link_seq, IPAddress ip);
  SeqProbMap *findmap(uint32_t seq);
  int expected_rx(uint32_t seq, IPAddress src);
  int neighbor_weight(IPAddress src);
  int get_wait_time(uint32_t, IPAddress);
  void start_flood(Packet *);
  void process_packet(Packet *);
  static void static_forward_hook(Timer *, void *e) { 
    ((FlashFlood *) e)->forward_hook(); 
  }
  bool get_prob(uint32_t seq, IPAddress, int *);
  bool set_prob(uint32_t seq, IPAddress, int);
};


CLICK_ENDDECLS
#endif
