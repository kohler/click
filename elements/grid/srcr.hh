#ifndef CLICK_SRCR_HH
#define CLICK_SRCR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ip6address.hh>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
#include "linktable.hh"
#include <click/vector.hh>
#include "arptable.hh"
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


enum SRCRPacketType { PT_QUERY = 0x11,
		     PT_REPLY = 0x22,
		     PT_DATA  = 0x33 };


// Packet format.
struct sr_pkt {
  uint8_t       ether_dhost[6];
  uint8_t       ether_shost[6];
  uint16_t      ether_type;

  uint8_t _version;

  uint8_t _type;  // PacketType
  uint16_t _flags; // PacketFlags
  
  // PT_QUERY
  click_in6_addr _qdst; // Who are we looking for?
  ulong _seq;   // Originator's sequence number.
  
  // PT_REPLY
  // The data is in the PT_QUERY fields.
  
  // PT_DATA
  uint16_t _dlen;
  
  // Route
  uint8_t _nhops;
  uint8_t _next;   // Index of next node who should process this packet.

  // How long should the packet be?
  size_t hlen_wo_data() const { return len_wo_data(_nhops); }
  size_t hlen_with_data() const { return len_with_data(_nhops, ntohs(_dlen)); }
  
  static size_t len_wo_data(int nhops) {
    return sizeof(struct sr_pkt) + nhops * sizeof(click_in6_addr) + nhops * sizeof(uint8_t)*2;
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
  IP6Address get_hop(int h) { 
    click_in6_addr *ndx = (click_in6_addr *) (ether_dhost + sizeof(struct sr_pkt));
    return IP6Address(ndx[h]);
  }
  uint8_t get_fwd_metric(int h) { 
    uint8_t *ndx = (uint8_t *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(click_in6_addr));
    return ndx[h];
  }

  uint8_t get_rev_metric(int h) { 
    uint8_t *ndx = (uint8_t *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(click_in6_addr) + num_hops() * sizeof(uint8_t));
    return ndx[h];
  }
  
  void  set_hop(int hop, IP6Address p) { 
    click_in6_addr *ndx = (click_in6_addr *) (ether_dhost + sizeof(struct sr_pkt));
    ndx[hop] = p.in6_addr();
  }
  void set_fwd_metric(int hop, uint8_t s) { 
    uint8_t *ndx = (uint8_t *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(click_in6_addr));
    ndx[hop] = s;
  }

  void set_rev_metric(int hop, uint8_t s) { 
    uint8_t *ndx = (uint8_t *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(click_in6_addr) + num_hops() * sizeof(uint8_t));
    ndx[hop] = s;
  }
  
  /* remember that if you call this you must have set the number of hops in this packet! */
  u_char *data() { return (ether_dhost + len_wo_data(num_hops())); }
  String s();
};



typedef Vector<IP6Address> Path;
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
  static String static_print_arp(Element *e, void *);
  String print_arp();


  void push(int, Packet *);
  
  void arp(IP6Address ip, EtherAddress en);
  Packet *encap(const u_char *payload, u_long len, Vector<IP6Address>);
private:

  IP6Address _ip;    // My IP address.
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
  IP6Address _ls_net;
  class ARPTable *_arp_table;
  
  u_short get_metric(IP6Address other);

  void update_link(IP6Pair p, u_short m, unsigned int now);
  void srcr_assert_(const char *, int, const char *) const;
};


CLICK_ENDDECLS
#endif
