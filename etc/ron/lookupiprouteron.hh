#ifndef LOOKUPIPROUTERON_HH
#define LOOKUPIPROUTERON_HH

/*
 * =c
 * LookupIPRouteRON([N])
 * =s IP, classification
 * Path selecting RON routing table.
 * =d
 * Input:
 * Forward IP packets(no ether header) on port 0.
 * Expects a destination IP address annotation with each packet.
 * Probes outgoing paths for unknown destinations. Selects path
 * with loweset latency for the new path. Emits packets on chosen port.
 *
 * Reverse IP packets(no ether header) on ports 1 -> N.
 * Reply packets from path I are pushed onto input port I.
 *
 * Output:
 * Forward path packets are output on the ports connected to the chosen path.
 * Reverse path packets output on port 0. Duplicate ACKs are filtered.
 *
 * N is the number of outgoing paths, including the direct path.  It is
 * optional; the correct number is taken from the configuration.
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

  // In POLICY_LOCAL, the local path is always chosen.
  static const int POLICY_LOCAL                 = 0;

  // In POLICY_RANDOM, a path is chosen at random out of all possible paths.
  static const int POLICY_RANDOM                = 1;

  // In POLICY_PROBE3, three random paths are chosen, and the path with shorted
  // rtt for the SYN/SYN-ACK is chosen.
  static const int POLICY_PROBE3                = 2;

  // In POLICY_PROBE3_UNPROBED, two least recently probed paths and the path with
  // previous shortest rtt are chosen to be probed.
  static const int POLICY_PROBE3_UNPROBED       = 3;




  // In POLICY_PROBE3_LOCAL, two random paths and the local path are chosen to
  // be probed. The path with the shorted rtt for the SYN/SYN-ACK is chosen.
  static const int POLICY_PROBE3_LOCAL          = 10;






  // In POLICY_PROBE3_UNPROBED_LOCAL, one least recently probed path, the path
  // with the shortest rtt, and the local path are chosen to be probed.
  static const int POLICY_PROBE3_UNPROBED_LOCAL = 5;

  // In POLICY_PROBE_ALL, all paths are probed in parallel.
  static const int POLICY_PROBE_ALL             = 11;



  static const int NUM_POLICIES = 4;

  LookupIPRouteRON();
  ~LookupIPRouteRON();

  const char *class_name() const		{ return "LookupIPRouteRON"; }
  const char *port_count() const		{ return "2-/="; }
  const char *processing() const		{ return PUSH; }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  void push(int inport, Packet *p);
  static void print_time(char* s);

  static int myrandom(int x);


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
  void send_rst(Packet *p, FlowTableEntry *match, int outport);

  void policy_handle_syn(FlowTableEntry *flow, Packet *p, bool first_syn);
  void policy_handle_synack(FlowTableEntry *flow, unsigned int port, Packet *p);


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
  unsigned long syn_seq;

  int policy;
  int probed_ports[32];
  long first_syn_sec, first_syn_usec;


  FlowTableEntry() {valid = 1; all_answered = 1;}

  bool is_old() const        { return (probe_time + ROUTE_TABLE_TIMEOUT
				       < click_jiffies());}
  bool is_pending() const    { return (outstanding_syns > 0);}
  bool is_valid() const      { return (valid); }
  bool is_active() const     { return ((forw_alive || rev_alive) &&
				       !is_pending() ); }
  void saw_first_syn();

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
      unsigned probe_time, unsigned syn_seq);

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

  struct ProbeInfo {
    int port_number;
    long last_probe_time;
    unsigned long rtt_sec, rtt_usec;
    bool first_syn;
    struct ProbeInfo *next;
  };

  IPAddress dst;
  unsigned outgoing_port;
  unsigned probe_time;
  struct ProbeInfo *probes; // more recent probes are closer to the front

  void add_probe_info(int port, long rtt_sec, long rtt_usec);

  // returns a port to probe. last recently used, excludes not1, not2 if greater than zero
  int  choose_least_recent_port(int noutputs, int not1, int not2);
  int  choose_fastest_port();
  void save_rtt(int port, long sec, long usec);
  void sent_probe(int port);

  void invalidate()           { outgoing_port = 0;}
  //unsigned get_age()          { return click_jiffies() - probe_time; }

  bool is_valid()             { return outgoing_port != 0; }
  //bool is_recent()            { return (probe_time + DST_TABLE_TIMEOUT >= click_jiffies());}

  bool is_recent() { return true; }
};

class LookupIPRouteRON::DstTable {
public:
  DstTable();
  ~DstTable();

  LookupIPRouteRON::DstTableEntry* lookup(IPAddress dst, bool only_valid);
  LookupIPRouteRON::DstTableEntry* insert(IPAddress dst, unsigned short assigned_port);
  void print();
  //void invalidate(IPAddress dst) {insert(dst, 0); }

private:
  int q;
  DstTableEntry *_last_entry;
  Vector<DstTableEntry> _v;
};



#endif



