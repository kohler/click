#ifndef LOOKUPIPROUTERON_HH
#define LOOKUPIPROUTERON_HH

/*
 * =c
 * LookupIPRouteRON(n)
 * =s IP, classification
 * Path selecting RON routing table. 
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

// These are arbitrary values
#define ROUTE_TABLE_TIMEOUT 30*100 // 30 seconds
#define DST_TABLE_TIMEOUT 60*100   // 60 seconds

class LookupIPRouteRON : public Element {
public:

  LookupIPRouteRON();
  ~LookupIPRouteRON();
  
  const char *class_name() const		{ return "LookupIPRouteRON"; }
  const char *processing() const		{ return PUSH; }
  LookupIPRouteRON *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  void push(int inport, Packet *p);



protected:
  class FlowTableEntry;
  class FlowTable;

  class DstTableEntry;
  class DstTable;

  void push_forward_packet(Packet *p);
  void push_forward_syn(Packet *p);
  void push_forward_fin(Packet *p);
  void push_forward_rst(Packet *p);
  void push_forward_normal(Packet *p);

  void push_reverse_packet(int inport, Packet *p);
  void push_reverse_synack(unsigned inport, Packet *p);
  void push_reverse_fin(Packet *p);
  void push_reverse_rst(Packet *p);
  void push_reverse_normal(Packet *p);  

  void duplicate_pkt(Packet *p);

private:
  FlowTable *_flow_table;
  DstTable  *_dst_table;
  Timer _expire_timer;

  /* OPTIMIZE: Dont need caching for now.
  IPAddress _last_addr;
  IPAddress _last_gw;
  int _last_output;
  */

  static void expire_hook(Timer*, void *thunk);
  
};

class LookupIPRouteRON::FlowTableEntry {
public:

  IPAddress src, dst;
  unsigned short sport, dport; // network order
  unsigned outgoing_port;
  unsigned oldest_unanswered, last_reply, probe_time;
  bool forw_alive, rev_alive;
  unsigned outstanding_syns;
  //Vector<Packet*> waiting;
  bool valid, all_answered;

  FlowTableEntry() {valid = 1; all_answered = 1;}

  bool is_old() const        { return (probe_time + ROUTE_TABLE_TIMEOUT
				       < click_jiffies());}
  //bool is_waiting() const    { return (waiting.size() > 0); } // waiting for a different probe
  bool is_pending() const    { return (outstanding_syns > 0);}
  bool is_valid() const      { return (valid); }
  bool is_active() const     { return ((forw_alive || rev_alive) && 
				       !is_pending() ); }
  
  unsigned get_age() {
    if (all_answered && is_active()) return 0; 
    else return click_jiffies() - oldest_unanswered;
  }

  void invalidate()          {valid = 0;}  
    // an entry is invalid if it cannot be used even if it's recent.
    // for example, if the port went down, the mapping would become invalid

  void saw_forward_packet() {
    if (all_answered) {
      oldest_unanswered = click_jiffies();
      all_answered = 0;
    }
  }

  void saw_reply_packet() {
    last_reply = click_jiffies();
    all_answered = 1;
  }
};


// ----- FlowTable internal class -----
class  LookupIPRouteRON::FlowTable {
public:
  FlowTable();
  ~FlowTable();

  // If exact match exists, returns a pointer to that FlowTableEntry
  // If no matching flow exists, return null
  LookupIPRouteRON::FlowTableEntry *
  lookup(IPAddress src, IPAddress dst,
	 unsigned short sport, unsigned short dport);
  
  // retuns a pointer to the added FlowTableEntry    
  //   outgoing_port is intialized to zero
  //   forw_alive, rev_alive are initialized to TRUE
  //   outstanding_syns is initialized to zero.
  LookupIPRouteRON::FlowTableEntry*
  add(IPAddress src, IPAddress dst, 
      unsigned short sport, unsigned short dport, 
      unsigned probe_time);

  void del(IPAddress src, IPAddress dst, 
	   unsigned short sport, unsigned short dport);
  //void clear()  { _v.clear(); }
  void print();

private:
  
  FlowTableEntry *_last_entry;
  Vector<FlowTableEntry> _v;
};    

class LookupIPRouteRON::DstTableEntry {
public:
  IPAddress dst;
  unsigned outgoing_port;
  unsigned probe_time;
  
  void invalidate()           { outgoing_port = 0;}  
  //unsigned get_age()          { return click_jiffies() - probe_time; }

  bool is_valid()             { return outgoing_port != 0; }
  bool is_recent()            { return (probe_time + DST_TABLE_TIMEOUT
					>= click_jiffies());}
};

class LookupIPRouteRON::DstTable {
public:
  DstTable();
  ~DstTable();

  LookupIPRouteRON::DstTableEntry* lookup(IPAddress dst);
  void insert(IPAddress dst, unsigned short assigned_port);
  void print();
  //void invalidate(IPAddress dst) {insert(dst, 0); }

private:
  int q;
  DstTableEntry *_last_entry;
  Vector<DstTableEntry> _v;
};



#endif



