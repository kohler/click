#ifndef FASTUDPSRCIP6_HH
#define FASTUDPSRCIP6_HH

/*
 * =c
 * FastUDPSourceIP6(RATE, LIMIT, LEN, SETHADDR, SIP6ADDR, SPORT, DETHADDR, DIP6ADDR, DPORT [, CHECKSUM?, INTERVAL, ACTIVE])
 * =s sources
 * creates packets with static UDP/IP6/Ethernet headers
 * =d
 * FastUDPSourceIP6 is a benchmark tool. At initialization
 * time, FastUDPSourceIP6 creates a UDP/IP6 packet of length
 * LEN (min 60), with source ethernet address SETHADDR,
 * source IP6 address SIP6ADDR, source port SPORT,
 * destination ethernet address DETHADDR, destination IP6
 * address DIP6ADDR, and destination port DPORT. The UDP
 * checksum is calculated if CHECKSUM? is true; it is
 * true by default. Each time the FastUDPSource element
 * is called, it increments the reference count on the
 * skbuff created and returns the skbuff object w/o
 * copying or cloning. Therefore, the packet returned by
 * FastUDPSource should not be modified.
 *
 * FastUDPSource sents packets at RATE packets per
 * second. It will send LIMIT number of packets in
 * total.
 *
 * After FastUDPSource has sent LIMIT packets, it will
 * calculate the average send rate (packets per second)
 * between the first and last packets sent and make that
 * available in the rate handler.
 *
 * By default FastUDPSource is ACTIVE.
 *
 * PACKET is zero by default. If it is not 0, after
 * PACKET number of packets, both sport and dport will
 * be incremented by 1. Checksum will be recomputed.
 *
 * =h count read-only
 * Returns the total number of packets that have been generated.
 * =h rate read/write
 * Returns or sets the RATE parameter.
 * =h reset write
 * Reset and restart.
 * =h active write
 * Change ACTIVE
 *
 * =e
 *  FastUDPSourceIP6(100000, 500000, 60, 0:0:0:0:0:0, 3ffe::1.0.0.1, 1234, 
 *                                    1:1:1:1:1:1, 3ff2::2.0.0.2, 1234) 
 *    -> ToDevice;
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/gaprate.hh>
#include <click/packet.hh>
#include <click/click_ether.h>
#include <click/click_udp.h>
#include <click/ip6address.hh>

class FastUDPSourceIP6 : public Element {

  bool _rate_limited; // obey _rate? rather than as fast as possible.
  unsigned _len;
  click_ether _ethh;
  IP6Address _sip6addr; 
  IP6Address _dip6addr;
  unsigned short _sport;
  unsigned short _dport;
  unsigned short _incr;
  unsigned int _interval;
  bool _cksum;
  WritablePacket *_packet;
  struct sk_buff *_skb;
  unsigned long _first; // jiffies
  unsigned long _last;

  void incr_ports();

 public:
  
  static const unsigned NO_LIMIT = 0xFFFFFFFFU;

  GapRate _rate;
  unsigned _count;
  unsigned _limit;
  bool _active;

  FastUDPSourceIP6();
  ~FastUDPSourceIP6();
  
  const char *class_name() const	{ return "FastUDPSourceIP6"; }
  const char *processing() const	{ return PULL; }
  
  FastUDPSourceIP6 *clone() const		{ return new FastUDPSourceIP6; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  Packet *pull(int);

  void add_handlers();
  void reset();
  unsigned count() { return _count; }
  unsigned long first() { return _first; }
  unsigned long last() { return _last; }

#if 0
  friend int FastUDPSourceIP6_limit_write_handler 
    (const String &, Element *e, void *, ErrorHandler *);
  friend int FastUDPSourceIP6_rate_write_handler 
    (const String &, Element *e, void *, ErrorHandler *);
  friend int FastUDPSourceIP6_active_write_handler 
    (const String &, Element *e, void *, ErrorHandler *);
#endif
};

#endif
