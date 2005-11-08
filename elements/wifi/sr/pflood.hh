#ifndef CLICK_PFLOOD_HH
#define CLICK_PFLOOD_HH
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
#include "pflood.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * PFlood(ETHTYPE eth, IP ip, BCAST_IP ip, ETH eth, P int, 
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
 * =item P
 * 
 * value of 0 to 100 where P is the probability that this
 * element will forward any particular packet. It will only
 * forward it once.
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


class PFlood : public Element {
 public:
  
  PFlood();
  ~PFlood();
  
  const char *class_name() const		{ return "PFlood"; }
  const char *port_count() const		{ return "2/2"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  static String static_print_debug(Element *f, void *);
  static String static_print_p(Element *f, void *);
  static int static_write_debug(const String &arg, Element *e,
				void *, ErrorHandler *errh); 

  static int static_write_p(const String &arg, Element *e,
				void *, ErrorHandler *errh); 

  static String static_print_stats(Element *e, void *);
  String print_stats();

  static String static_print_packets(Element *e, void *);
  String print_packets();

  void push(int, Packet *);

  void add_handlers();
private:


  class Broadcast {
  public:
    uint32_t _seq;
    bool _originated; /* this node started the bcast */
    Packet *_p;
    int _num_rx;
    Timestamp _first_rx;
    bool _forwarded;
    bool _actually_sent;
    Timer *t;
    Timestamp _to_send;

    void del_timer() {
      if (t) {
	t->unschedule();
	delete t;
	t = NULL;
      }
    }
  };


  DEQueue<Broadcast> _packets;

  IPAddress _ip;    // My IP address.
  EtherAddress _en; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype
  IPAddress _bcast_ip;

  EtherAddress _bcast;

  bool _debug;


  int _packets_originated;
  int _packets_tx;
  int _packets_rx;

  int _p;
  int _max_delay_ms;

  int _history;

  void forward(Broadcast *bcast);
  void forward_hook();
  void trim_packets();
  static void static_forward_hook(Timer *, void *e) { 
    ((PFlood *) e)->forward_hook(); 
  }
};


CLICK_ENDDECLS
#endif
