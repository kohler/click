#ifndef CLICK_SRCR_HH
#define CLICK_SRCR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include "linktable.hh"
#include <click/vector.hh>
#include "arptable.hh"
CLICK_DECLS

/*
 * =c
 * SRCR(ETHERTYPE, IP, ETH, LinkTable element, ARPTable element)
 * =d
 * DSR-inspired ad-hoc routing protocol.
 * Input 0: Incoming ethernet packets
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

  u_char _type;  // PacketType
  u_char _flags; // PacketFlags
  
  // PT_QUERY
  in_addr _qdst; // Who are we looking for?
  u_long _seq;   // Originator's sequence number.
  
  // PT_REPLY
  // The data is in the PT_QUERY fields.
  
  // PT_DATA
  u_short _dlen;
  
  // Route
  u_short _nhops;
  u_short _next;   // Index of next node who should process this packet.
  u_short _gateway;

  // How long should the packet be?
  size_t hlen_wo_data() const { return len_wo_data(ntohs(_nhops)); }
  size_t hlen_with_data() const { return len_with_data(ntohs(_nhops), ntohs(_dlen)); }
  
  static size_t len_wo_data(int nhops) {
    return sizeof(struct sr_pkt) + nhops * sizeof(in_addr) + nhops * sizeof(u_short);
  }
  static size_t len_with_data(int nhops, int dlen) {
    return len_wo_data(nhops) + dlen;
  }
  
  u_short num_hops() {
    return ntohs(_nhops);
  }

  u_short next() {
    return ntohs(_next);
  }

  void set_next(u_short n) {
    _next = htons(n);
  }

  void set_num_hops(u_short n) {
    _nhops = htons(n);
  }

  in_addr get_hop(int h) { 
    in_addr *ndx = (in_addr *) (ether_dhost + sizeof(struct sr_pkt));
    return ndx[h];
  }
  u_short get_metric(int h) { 
    u_short *ndx = (u_short *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(in_addr));
    return ntohs(ndx[h]);
  }
  
  void  set_hop(int hop, in_addr s) { 
    in_addr *ndx = (in_addr *) (ether_dhost + sizeof(struct sr_pkt));
    ndx[hop] = s;
  }
  void set_metric(int hop, u_short s) { 
    u_short *ndx = (u_short *) (ether_dhost + sizeof(struct sr_pkt) + num_hops() * sizeof(in_addr));
    ndx[hop] = htons(s);
  }
  
  /* remember that if you call this you must have set the number of hops in this packet! */
  u_char *data() { return (ether_dhost + len_wo_data(num_hops())); }
  String s();
};











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
  static String static_print_arp(Element *e, void *);
  String print_arp();


  void push(int, Packet *);
  
  void arp(IPAddress ip, EtherAddress en);
  static Packet *encap(const u_char *payload, u_long len, Vector<IPAddress>);
private:

  IPAddress _ip;    // My IP address.
  EtherAddress _eth; // My ethernet address.
  uint16_t _et;     // This protocol's ethertype
  // Statistics for handlers.
  int _datas;
  int _databytes;


  EtherAddress _bcast;
  class LinkTable *_link_table;
  class LinkStat *_link_stat;
  class ARPTable *_arp_table;
  
  u_short get_metric(IPAddress other);

  void update_link(IPPair p, u_short m, unsigned int now);
  void srcr_assert_(const char *, int, const char *) const;
};


CLICK_ENDDECLS
#endif
