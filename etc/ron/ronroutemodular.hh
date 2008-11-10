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
#include <click/string.hh>

class RONRouteModular : public Element {
public:
  class Policy;
  class FlowTable;
  class FlowTableEntry;

  RONRouteModular();
  ~RONRouteModular();

  const char *class_name() const		{ return "RONRouteModular"; }
  const char *port_count() const		{ return "-/="; }
  const char *processing() const		{ return PUSH; }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  void push(int inport, Packet *p);
  void send_rst(Packet *p, unsigned long seq, int outport);

protected:
  FlowTable *_flowtable;
  Vector<Policy*> _policies;

  void push_forward_packet(Packet *p);
  void push_reverse_packet(int inport, Packet *p);

  void duplicate_pkt(Packet *p);
  static int myrandom(int x);
  static void print_time(char* s);

private:
  static void expire_hook(Timer*, void *thunk);
};



class RONRouteModular::Policy{

protected:
  RONRouteModular *_parent;
  int _numpaths;

public:
  Policy(RONRouteModular *parent) {_parent = parent;}
  virtual ~Policy() {}

  virtual void initialize(int numpaths){_numpaths = numpaths;}

  virtual void push_forward_syn(Packet *p) = 0;
  virtual void push_forward_fin(Packet *p) = 0;
  virtual void push_forward_rst(Packet *p) = 0;
  virtual void push_forward_normal(Packet *p) = 0;

  virtual void push_reverse_synack(int inport, Packet *p) = 0;
  virtual void push_reverse_fin(Packet *p) = 0;
  virtual void push_reverse_rst(Packet *p) = 0;
  virtual void push_reverse_normal(Packet *p) = 0;
};

class RONRouteModular::FlowTableEntry {
public:
  IPAddress src, dst;
  unsigned short sport, dport; // network order
  int policy;

  FlowTableEntry(IPAddress s, unsigned short sp,
		 IPAddress d, unsigned short dp, int p) {
    src = s; sport = sp;
    dst = d; dport = dp;
    policy = p;
  }
  bool match(IPAddress s, unsigned short sp,
	     IPAddress d, unsigned short dp) {
    return ((src == s) && (dst == d) && (sport == sp) && (dport == dp));
  }
};

class RONRouteModular::FlowTable {
protected:
  Vector<FlowTableEntry> _v;

public:
  FlowTable(){}

  RONRouteModular::FlowTableEntry *
  insert(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport, int policy);

  RONRouteModular::FlowTableEntry *
  lookup(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport);

  void
  remove(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport);

};
#endif




