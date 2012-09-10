#ifndef FASTUDPFLOWS_HH
#define FASTUDPFLOWS_HH

/*
 * =c
 * FastUDPFlows(RATE, LIMIT, LEN,
 *              SRCETH, SRCIP,
 *              DSTETH, DSTIP,
 *              FLOWS, FLOWSIZE [, CHECKSUM, ACTIVE])
 * =s udp
 * creates packets flows with static UDP/IP/Ethernet headers
 * =d
 * FastUDPFlows is a benchmark tool. At initialization time, FastUDPFlows
 * creates FLOWS number of UDP/IP packets of length LENGTH (min 60), with source
 * ethernet address SRCETH, source IP address SRCIP, destination ethernet
 * address DSTETH, and destination IP address DSTIP. Source and
 * destination ports are randomly generated. The UDP checksum is calculated if
 * CHECKSUM is true; it is true by default. Each time the FastUDPFlows
 * element is called, it selects a flow, increments the reference count on the
 * skbuff created and returns the skbuff object w/o copying or cloning.
 * Therefore, the packet returned by FastUDPFlows should not be modified.
 *
 * FastUDPFlows sents packets at RATE packets per second. It will send LIMIT
 * number of packets in total. Each flow is limited to FLOWSIZE number of
 * packets. After FLOWSIZE number of packets are sent, the sort and dst port
 * will be modified.
 *
 * After FastUDPFlows has sent LIMIT packets, it will calculate the average
 * send rate (packets per second) between the first and last packets sent and
 * make that available in the rate handler.
 *
 * By default FastUDPFlows is ACTIVE.
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
 *  FastUDPFlows(100000, 500000, 60,
 *               0:0:0:0:0:0, 1.0.0.1, 1234,
 *               1:1:1:1:1:1, 2.0.0.2, 1234,
 *               100, 10)
 *    -> ToDevice;
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/gaprate.hh>
#include <click/packet.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>

class FastUDPFlows : public Element {

  bool _rate_limited;
  unsigned _len;
  click_ether _ethh;
  struct in_addr _sipaddr;
  struct in_addr _dipaddr;
  unsigned int _nflows;
  unsigned int _last_flow;
  unsigned int _flowsize;
  bool _cksum;
  click_jiffies_t _first;
  click_jiffies_t _last;

  struct flow_t {
      Packet *packet;
      int flow_count;
  };
  flow_t *_flows;
  void change_ports(int);
  Packet *get_packet();

 public:

  static const unsigned NO_LIMIT = 0xFFFFFFFFU;

  GapRate _rate;
  unsigned _count;
  unsigned _limit;
  bool _active;

  FastUDPFlows() CLICK_COLD;
  ~FastUDPFlows() CLICK_COLD;

  const char *class_name() const	{ return "FastUDPFlows"; }
  const char *port_count() const	{ return PORTS_0_1; }
  const char *processing() const	{ return PULL; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  Packet *pull(int);

  void add_handlers() CLICK_COLD;
  void reset();
  unsigned count() { return _count; }
  click_jiffies_t first() { return _first; }
  click_jiffies_t last() { return _last; }
};


#endif
