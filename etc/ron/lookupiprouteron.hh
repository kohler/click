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

#define ROUTE_TIMEOUT 30*100
#define WAIT_TIMEOUT 3*100   // These are arbitrary values

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
  class TableEntry;
  class IPTableRON;

  static const int EXACT_ACTIVE_RECENT   = 1;
  static const int EXACT_ACTIVE_OLD      = 2;
  static const int EXACT_PENDING         = 3;
  static const int EXACT_WAITING         = 4;

  static const int EXACT_INACTIVE_RECENT = 5; // **
  static const int EXACT_INACTIVE_OLD    = 6; // ** 

  static const int SIMILAR_ACTIVE_RECENT = 7;

  static const int SIMILAR_INACTIVE_RECENT=8; // **

  static const int SIMILAR_ACTIVE_OLD    = 9;
  static const int SIMILAR_PENDING       = 10;
  static const int SIMILAR_WAITING       = 11;
  static const int NOMATCH               = 12;

  void push_forward_packet(Packet *p);
  void push_forward_syn(Packet *p);
  void push_forward_fin(Packet *p);
  void push_forward_rst(Packet *p);
  void push_forward_normal(Packet *p);

  void push_reverse_packet(int inport, Packet *p);
  void push_reverse_synack(int inport, Packet *p);
  void push_reverse_fin(int inport, Packet *p);
  void push_reverse_rst(int inport, Packet *p);
  void push_reverse_normal(int inport, Packet *p);  

  void duplicate_pkt(Packet *p);

private:
  IPTableRON *_t;
  Timer _expire_timer;

  /* OPTIMIZE: Dont need caching for now.
  IPAddress _last_addr;
  IPAddress _last_gw;
  int _last_output;
  */

  static void expire_hook(Timer*, void *thunk);
  
};

class LookupIPRouteRON::TableEntry {
public:
  // note: this order matters
  static const int ACTIVE_RECENT = 20;
  static const int INACTIVE_RECENT=21;

  static const int PENDING       = 22;
  static const int WAITING       = 23;

  static const int ACTIVE_OLD    = 24;
  static const int INACTIVE_OLD  = 25;
  static const int INVALID       = 26;

  IPAddress src, dst;
  unsigned short sport, dport; // network order
  unsigned outgoing_port;
  unsigned oldest_unanswered, last_reply, probe_time;
  bool forw_alive, rev_alive;
  unsigned outstanding_syns;
  Vector<Packet*> waiting;
  bool valid, all_answered;

  TableEntry() {valid = 1; all_answered = 1;}

  bool is_old() const        { return (probe_time + ROUTE_TIMEOUT
				       < click_jiffies());}
  bool is_waiting() const    { return (waiting.size() > 0); } // waiting for a different probe
  bool is_pending() const    { return (outstanding_syns > 0);}
  bool is_valid() const      { return (valid); }
  bool is_active() const     { return ((forw_alive || rev_alive) && 
				       !is_waiting() && !is_pending() ); }
  
  unsigned get_age() {return click_jiffies() - oldest_unanswered; }

  int get_state() const {
    if (is_active()) {
      if (!is_old())
	return ACTIVE_RECENT;
      else 
	return ACTIVE_OLD;
    } else if (is_pending())
      return PENDING;
    else if (is_waiting()) 
      return WAITING;
    else if (!valid)
      return INVALID;

    else if (!is_old()) {
      return INACTIVE_OLD;
    } else {
      return INACTIVE_RECENT;
    }
  }
  void invalidate()          {valid = 0;}  
    // an entry is invalid if it cannot be used even if it's recent.
    // for example, if the port went down, the mapping would become invalid

  // pushes all waiting packets & clears wait list
  void push_all_waiting(const Element::Port p) {
    for(int i=0; i<waiting.size(); i++) {
      p.push(waiting[i]);
    }
    waiting.clear();
  }

  // pushes firstwaiting packets & clears wait list
  Packet *get_first_waiting() {
    Packet *p;
    if (waiting.size() > 0) {
      p = waiting[0]; 

      for(int i=0; i<waiting.size()-1; i++) {
	waiting[i] = waiting[i+1];
      }
      waiting.pop_back();
    }
    return p;
  }

  void clear_waiting() {
    for(int i=0; i<waiting.size(); i++) {
      waiting[i]->kill();
    }
    waiting.clear();
  }

  void add_waiting(Packet *p) {
    waiting.push_back(p);
  }
  
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


// ----- IPTableRON internal class -----
class  LookupIPRouteRON::IPTableRON {
public:
  IPTableRON();
  ~IPTableRON();

  // Inserts TableEntries where:
  //   - The first waiting pkt is a SYN
  //   - and (the dst has no pending probes OR 
  //     this SYN has been waiting too long)
  // If there are similar TableEntries that match, only insert the
  // oldest one.
  void get_waiting_syns(Vector<TableEntry*> *t);

  // If exact match exists, return state of match, and <entry> points to match
  // If similar flow exists, return state of similiar match, 
  //    and <entry> points to it.
  // If no similar flows exist, return NOMATCH, and <entry> is null
  int lookup(IPAddress src, IPAddress dst,
	     unsigned short sport, unsigned short dport,
	     struct LookupIPRouteRON::TableEntry **entry);
  
  // retuns a pointer to the added TableEntry    
  //   outgoing_port is intialized to zero
  //   forw_alive, rev_alive are initialized to TRUE
  //   outstanding_syns is initialized to zero.
  LookupIPRouteRON::TableEntry*
  add(IPAddress src, IPAddress dst, 
      unsigned short sport, unsigned short dport, 
      //unsigned outgoing_port,
      //unsigned oldest_unanswered, unsigned last_reply, 
      unsigned probe_time);
      //bool forw_alive, bool rev_alive, unsigned outstanding_syns);

  // frees all waiting packets from similar flows to <dst>
  void send_similar_waiting(IPAddress dst, const Element::Port p);

  void del(IPAddress dst);
  void clear()  { _v.clear(); }

  void print();

private:
  
  Vector<TableEntry> _v;
  
};    
#endif



