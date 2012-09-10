#ifndef CLICK_SPANTREE_HH
#define CLICK_SPANTREE_HH
#include <click/element.hh>
#include "bridgemessage.hh"
#include <click/timer.hh>
CLICK_DECLS
class Suppressor;
class EtherSwitch;

/**
=c

EtherSpanTree(ADDR, INPUT_SUPPRESSOR, OUTPUT_SUPPRESSOR, SWITCH)

=s ethernet

802.1d Ethernet spanning tree implementation

=d

Implements the IEEE 802.1d spanning tree algorithm for Ethernet switches.
Expects 802.1d control packets on its inputs and reacts by selectively
suppressing forwarding on an associated EtherSwitch.

ADDR is the address of this Ethernet switch.  SWITCH is the name of an
EtherSwitch element that actually switches packets.  INPUT_SUPPRESSOR and
OUTPUT_SUPPRESSOR are two Suppressor elements; they should be placed upstream
and downstream of the SWITCH.  The EtherSpanTree, Suppressor, and EtherSwitch
elements should all have the same numbers of inputs and outputs, equal to the
number of ports in the switch.

=e

  from_port0, from_port1 :: FromDevice...;
  to_port0, to_port1 :: ToDevice...;

  span_tree :: EtherSpanTree(00-1f-29-4d-f8-31, in_supp, out_supp, switch);
  switch :: EtherSwitch;
  in_supp, out_supp :: Suppressor;

  from_port0 -> c0 :: Classifier(14/4242, -); // ethertype 802.1d, others
  from_port1 -> c1 :: Classifier(14/4242, -);

  q0 :: Queue -> to_port0;
  q1 :: Queue -> to_port1;

  c0 [0] -> [0] span_tree [0] -> q0;
  c1 [0] -> [1] span_tree [1] -> q1;

  c0 [1] -> [0] in_supp [0] -> [0] switch [0] -> [0] out_supp [0] -> q0;
  c1 [1] -> [1] in_supp [1] -> [1] switch [1] -> [1] out_supp [1] -> q1;

=a

EtherSwitch, Suppressor
*/
class EtherSpanTree : public Element {

public:
  EtherSpanTree() CLICK_COLD;
  ~EtherSpanTree() CLICK_COLD;

  const char *class_name() const		{ return "EtherSpanTree"; }
  const char *port_count() const		{ return "-/="; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  static String read_msgs(Element* f, void *);
  void add_handlers() CLICK_COLD;


  void periodic();

  bool expire();
  void find_best();
  void find_tree();		// Returns true iff there is a change

  void push(int port, Packet* p);
  Packet* generate_packet(int output);

private:
  Suppressor* _input_sup;
  Suppressor* _output_sup;
  EtherSwitch* _switch;
  Timestamp* _topology_change;	// If set, tc should be sent with messages.
  bool _send_tc_msg;		// If true, tcm should be sent to root port.

  uint64_t _bridge_id;		// Should be 48 bits

  uint16_t _bridge_priority;	// High == unlikely to become the root node
  uint16_t _long_cache_timeout; // in seconds

  uint8_t _addr[6];

  BridgeMessage _best;


  // Do not change the order of the PortState enum tags.  (see set_state())
  enum PortState {BLOCK, LISTEN, LEARN, FORWARD};
  struct PortInfo {
    PortState state;
    Timestamp since;		// When the port entered the state
    bool needs_tca;
    BridgeMessage msg;
    PortInfo() { state = BLOCK; needs_tca = false; }
  };

  bool set_state(int i, PortState state); // Only expects BLOCK or FORWARD

  Vector<PortInfo> _port;

  Timer _hello_timer;
  static void hello_hook(Timer *, void *);

};

CLICK_ENDDECLS
#endif
