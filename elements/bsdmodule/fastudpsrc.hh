#ifndef FASTUDPSRC_HH
#define FASTUDPSRC_HH

/*
 * =c
 * FastUDPSource(RATE, LIMIT, LEN, SETHADDR, SIPADDR, SPORT, DETHADDR, DIPADDR, DPORT [, CHECKSUM?, INTERVAL, ACTIVE])
 * =s sources
 * creates packets with static UDP/IP/Ethernet headers
 * =d
 * FastUDPSource is a benchmark tool. At initialization
 * time, FastUDPSource creates a UDP/IP packet of length
 * LEN (min 60), with source ethernet address SETHADDR,
 * source IP address SIPADDR, source port SPORT,
 * destination ethernet address DETHADDR, destination IP
 * address DIPADDR, and destination port DPORT. The UDP
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
 *  FastUDPSource(100000, 500000, 60, 0:0:0:0:0:0, 1.0.0.1, 1234, 
 *                                    1:1:1:1:1:1, 2.0.0.2, 1234) 
 *    -> ToDevice;
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/gaprate.hh>
#include <click/packet.hh>
#include <click/click_ether.h>
#include <click/click_udp.h>

class FastUDPSource : public Element {

  bool _rate_limited; // obey _rate? rather than as fast as possible.
  unsigned _len;
  click_ether _ethh;
  struct in_addr _sipaddr;
  struct in_addr _dipaddr;
  unsigned short _sport;
  unsigned short _dport;
  unsigned short _incr;
  unsigned int _interval;
  bool _cksum;
  WritablePacket *_packet;
  unsigned long _first; // jiffies
  unsigned long _last;

  void incr_ports();

 public:
  
  static const unsigned NO_LIMIT = 0xFFFFFFFFU;

  GapRate _rate;
  unsigned _count;
  unsigned _limit;
  bool _active;

  FastUDPSource();
  ~FastUDPSource();
  
  const char *class_name() const	{ return "FastUDPSource"; }
  const char *processing() const	{ return PULL; }
  
  FastUDPSource *clone() const		{ return new FastUDPSource; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  Packet *pull(int);

  void add_handlers();
  void reset();
  unsigned count() { return _count; }
  unsigned long first() { return _first; }
  unsigned long last() { return _last; }

#if 0
  friend int FastUDPSource_limit_write_handler 
    (const String &, Element *e, void *, ErrorHandler *);
  friend int FastUDPSource_rate_write_handler 
    (const String &, Element *e, void *, ErrorHandler *);
  friend int FastUDPSource_active_write_handler 
    (const String &, Element *e, void *, ErrorHandler *);
#endif
};

#endif
