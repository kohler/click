#ifndef FLOODLOCQUERIER_HH
#define FLOODLOCQUERIER_HH

/*
 * =c
 * FloodingLocQuerier(E, I)
 * =s
 * Sets Grid Destination location by running a flooding query protocol
 *
 * =d
 *
 * Argument I should be this host's IP address, E should be this host's
 * Ethernet address, and LOCINFO is a GridLocationInfo element that knows
 * where this host is.
 *
 * Expects GRID_NBR_ENCAP packets with MAC headers on input 0. If a
 * location is already known for the destination, the destination
 * location field is filled in and the packet is sent to output 0.
 * Otherwise the packet is saved and a flooding location query is sent
 * to output 1 instead, ready for FixSrcLoc, checksumming and
 * transmission.  If a query response arrives on input 1 for a
 * location that we need, the mapping is recorded and the saved Grid
 * packet is sent through output 0.
 *
 * Input 1 expects flooding query packets, and query reply response
 * packets for us.  Packets must include all headers (MAC, Grid, etc.)
 *
 *
 * =a
 * LocQueryResponder, GridLocationInfo, SimpleLocQuerier */

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/timer.hh>
#include "elements/grid/gridlocationinfo.hh"
#include <click/bighashmap.hh>

class FloodingLocQuerier : public Element {
 public:
  
  FloodingLocQuerier();
  ~FloodingLocQuerier();
  
  const char *class_name() const		{ return "FloodingLocQuerier"; }
  const char *processing() const		{ return PUSH; }
  void add_handlers();
  
  FloodingLocQuerier *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void push(int port, Packet *);
  
  struct LocEntry {
    IPAddress ip;
    grid_location loc;
    unsigned short loc_err;
    bool loc_good;
    unsigned int loc_seq_no;;
    int last_response_jiffies;
    unsigned ok: 1;
    unsigned polling: 1;
    Packet *p;
    struct LocEntry *next;
  };

  // statistics
  unsigned int _loc_queries;
  unsigned int _pkts_killed;
  

 private:

  typedef BigHashMap<IPAddress, unsigned int> seq_map;
  seq_map _query_seqs;

  static const int NMAP = 256;
  LocEntry *_map[NMAP];

  EtherAddress _my_en;
  IPAddress _my_ip;
  Timer _expire_timer;

  void send_query_for(const IPAddress &);
  
  void handle_nbr_encap(Packet *);
  void handle_reply(Packet *);
  void handle_query(Packet *);

  static const int EXPIRE_TIMEOUT_MS = 15 * 1000;
  static void expire_hook(unsigned long);
  static String read_table(Element *, void *);
  
};

#endif
