#ifndef RONROUTEMODULAR_HH
#define RONROUTEMODULAR_HH

/*
 * =c
 * RonRouteModular(n)
 * =s IP, classification
 * Path selecting RON routing table. with modular policies
 * =d
 * Input: 
 * Forward IP packets(no ether header) on port 0.
 * Expects a destination IP address annotation with each packet.
 * Probes outgoing paths for unknown destinations. Selects path
 * with loweset latency for the new path. Emits packets on chosen port.
 *
 * Reverse IP packets(no ether header) on ports 1 -> n.
 * Reply packets from path i are pushed onto input port i. 
 *
 * Output:
 * Forward path packets are output on the ports connected to the chosen path.
 * Reverse path packets output on port 0. Duplicate ACKs are filtered.
 *
 * n is the number of outgoing paths, including the direct path.
 *
 * =a LookupIPRoute2, LookupIPRouteLinux
 */

#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

class RONRouteModular : public Element {
public:

  RONRouteModular();
  ~RONRouteModular();
  
  const char *class_name() const		{ return "RONRouteModular"; }
  const char *processing() const		{ return PUSH; }
  RONRouteModular *clone() const;
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  void push(int inport, Packet *p);
  static void print_time(char* s);

  static int myrandom(int x);


protected:

  void duplicate_pkt(Packet *p);
  void send_rst(Packet *p, FlowTableEntry *match, int outport);

private:

  static void expire_hook(Timer*, void *thunk);  
};

#endif



