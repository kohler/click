#ifndef CLICK_SRCR_HH
#define CLICK_SRCR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
#include <elements/grid/linktable.hh>
#include <click/vector.hh>
#include "ett.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * SRCR(ETHERTYPE, IP, ETH, ARPTable element, LT LinkTable element)
 * =d
 * DSR-inspired ad-hoc routing protocol.
 * Input 0: Incoming ethernet packets for me
 * Input 1: Incoming sniffed ethernet packets
 * Output 0: Outgoing ethernet packets
 * Output 1: IP packets for higher layer
 *
 */


enum SRCRPacketType { PT_QUERY = (1<<0),
		      PT_REPLY = (1<<1),
		      PT_DATA  = (1<<2) };



static const uint8_t _srcr_version = 0x02;

// Packet format.
struct sr_pkt {
  uint8_t       ether_dhost[6];
  uint8_t       ether_shost[6];
  uint16_t      ether_type;

  uint8_t _version; /* see _srcr_version */
  uint32_t _link_seq;
  uint8_t _type;  /* see enum SRCRPacketType */
  uint16_t _flags; 

  // PT_DATA
  uint16_t _dlen;
  
  // PT_QUERY
  in_addr _qdst; // Who are we looking for?
  
  in_addr extra1;
  in_addr extra2;
  uint8_t extra_fwd;
  uint8_t extra_rev;
  
  uint32_t _seq;   // Originator's sequence number.

  uint8_t _nhops;
  uint8_t _next;   // Index of next node who should process this packet.


  EtherAddress get_dhost() {
    return EtherAddress(ether_dhost);
  }
  EtherAddress get_shost() {
    return EtherAddress(ether_shost);
  }
  void set_dhost(EtherAddress _eth) {
    memcpy(ether_dhost, _eth.data(), 6);
  }
  void set_link_seq(int x) {
    _link_seq = htonl(x);
  }
  int get_link_seq() {
    return htonl(_link_seq);
  }
  void set_shost(EtherAddress _eth) {
    memcpy(ether_shost, _eth.data(), 6);
  }
  // How long should the packet be?
  size_t hlen_wo_data() const { return len_wo_data(_nhops); }
  size_t hlen_with_data() const { return len_with_data(_nhops, ntohs(_dlen)); }
  
  static size_t len_wo_data(int nhops) {
    return sizeof(struct sr_pkt) + nhops * sizeof(in_addr) + nhops * sizeof(uint8_t)*2;
  }
  static size_t len_with_data(int nhops, int dlen) {
    return len_wo_data(nhops) + dlen;
  }
  
  uint8_t num_hops() {
    return _nhops;
  }

  uint8_t next() {
    return _next;
  }

  void set_next(uint8_t n) {
    _next = n;
  }

  void set_num_hops(uint8_t n) {
    _nhops = n;
  }
  void set_data_len(uint16_t len) {
    _dlen = htons(len);
  }
  uint16_t data_len() {
    return ntohs(_dlen);
  }
  uint8_t get_fwd_metric(int h) { 
    uint8_t *ndx = (uint8_t *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(in_addr));
    return ndx[h];
  }

  uint8_t get_rev_metric(int h) { 
    uint8_t *ndx = (uint8_t *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(in_addr) + num_hops() * sizeof(uint8_t));
    return ndx[h];
  }
  IPAddress get_hop(int h) { 
    in_addr *ndx = (in_addr *) (this + 1);
    return IPAddress(ndx[h]);
  }
  
  void  set_hop(int hop, IPAddress p) { 
    in_addr *ndx = (in_addr *) (this + 1);
    ndx[hop] = p.in_addr();
  }
  void set_fwd_metric(int hop, uint8_t s) { 
    uint8_t *ndx = (uint8_t *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(in_addr));
    ndx[hop] = s;
  }

  void set_rev_metric(int hop, uint8_t s) { 
    uint8_t *ndx = (uint8_t *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(in_addr) + num_hops() * sizeof(uint8_t));
    ndx[hop] = s;
  }
  
  /* remember that if you call this you must have set the number of hops in this packet! */
  u_char *data() { return (ether_dhost + len_wo_data(num_hops())); }
  String s();
};



typedef Vector<IPAddress> Path;
inline unsigned
hashcode(const Path &p)
{
  unsigned h = 0;
  for (int x = 0; x < p.size(); x++) {
    h = h ^ hashcode(p[x]);
  }
  return h;
}

inline bool
operator==(const Path &p1, const Path &p2)
{
  if (p1.size() != p2.size()) {
    return false;
  }
  for (int x = 0; x < p1.size(); x++) {
    if (p1[x] != p2[x]) {
      return false;
    }
  }
  return true;
}

inline String path_to_string(const Path &p) 
{
  StringAccum sa;
  for(int x = 0; x < p.size(); x++) {
    sa << p[x].s().cc();
    if (x != p.size() - 1) {
      sa << " ";
    }
  }
  return sa.take_string();
}



class SRCR : public Element {
 public:
  
  SRCR();
  ~SRCR();
  
  const char *class_name() const		{ return "SRCR"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  SRCR *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);

  /* handler stuff */
  void add_handlers();

  static String static_print_stats(Element *e, void *);
  String print_stats();
  static String static_print_routes(Element *e, void *);
  String print_routes();

  void push(int, Packet *);
  
  Packet *encap(const u_char *payload, u_long len, Vector<IPAddress>);
private:

  IPAddress _ip;    // My IP address.
  EtherAddress _eth; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype
  // Statistics for handlers.
  int _datas;
  int _databytes;
  
  class PathInfo {
  public:
    Path _p;
    int _count;
    PathInfo() {
      memset(this, 0, sizeof(*this));
    }
  };

  typedef BigHashMap<Path, PathInfo> PathTable;
  typedef PathTable::const_iterator PIter;
  
  class PathTable _paths;
  EtherAddress _bcast;
  class LinkTable *_link_table;
  class LinkStat *_link_stat;
  IPAddress _ls_net;
  class RXStats *_rx_stats;
  class ARPTable *_arp_table;
  class ETT *_ett;
  
  int get_metric_to(IPAddress other);
  int get_metric_from(IPAddress other);

  void update_link(IPAddress from, IPAddress to, int metric);
  void srcr_assert_(const char *, int, const char *) const;
};


CLICK_ENDDECLS
#endif
