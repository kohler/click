#ifndef CLICK_SRPAKCET_HH
#define CLICK_SRPAKCET_HH
#include <click/ipaddress.hh>


enum SRCRPacketType { PT_QUERY = (1<<0),
		      PT_REPLY = (1<<1),
		      PT_DATA  = (1<<2),
                      PT_GATEWAY = (1<<3)};



enum SRCRPacketFlags {
  FLAG_ERROR = (1<<0),
  FLAG_UPDATE = (1<<0),
};

static const uint8_t _sr_version = 0x06;

// Packet format.
struct srpacket {
  uint8_t _version; /* see _srcr_version */
  uint8_t _type;  /* see enum SRCRPacketType */
  uint8_t _nhops;
  uint8_t _next;   // Index of next node who should process this packet.


  uint16_t _ttl;
  uint16_t _cksum;
  uint16_t _flags; 
  uint16_t _dlen;

  uint16_t _random_metric;

  // PT_QUERY
  uint32_t _qdst; // Who are we looking for?
  uint32_t _random_from;
  uint32_t _random_to;
  uint32_t _seq;   // Originator's sequence number.

  
  


  void set_random_from(IPAddress ip) {
    _random_from = ip;
  }
  void set_random_to(IPAddress ip) {
    _random_to = ip;
  }
  void set_random_metric(uint16_t m) {
    _random_metric = m;
  }
  IPAddress get_random_from() {
    return _random_from;
  }
  IPAddress get_random_to() {
    return _random_to;
  }
  int get_random_metric() {
    return _random_metric;
  }


  // How long should the packet be?
  size_t hlen_wo_data() const { return len_wo_data(_nhops); }
  size_t hlen_with_data() const { return len_with_data(_nhops, ntohs(_dlen)); }
  
  static size_t len_wo_data(int nhops) {
    return sizeof(struct srpacket) + nhops * sizeof(uint32_t) + (nhops) * sizeof(uint16_t);
  }
  static size_t len_with_data(int nhops, int dlen) {
    return len_wo_data(nhops) + dlen;
  }
  
  int num_hops() {
    return _nhops;
  }

  int next() {
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
  uint16_t get_metric(int h) { 
    uint16_t *ndx = (uint16_t *) (this+1);
    return ndx[h + num_hops()*2];
  }

  void set_metric(int hop, uint16_t s) { 
    uint16_t *ndx = (uint16_t *) (this+1);
    ndx[hop + num_hops()*2] = s;
  }



  IPAddress get_hop(int h) { 
    in_addr *ndx = (in_addr *) (this + 1);
    return IPAddress(ndx[h]);
  }
  
  void  set_hop(int hop, IPAddress p) { 
    in_addr *ndx = (in_addr *) (this + 1);
    ndx[hop] = p.in_addr();
  }
  
  /* remember that if you call this you must have set the number of hops in this packet! */
  u_char *data() { return (((u_char *)this) + len_wo_data(num_hops())); }
  String s();
};




struct extra_link_info {
  uint8_t _nhops;
  uint8_t _foo1;
  uint16_t _foo2;
  static int len(int num_hosts) {
    return sizeof(struct extra_link_info) + num_hosts * sizeof(uint32_t) 
      + (num_hosts) * sizeof(uint16_t);
  }
  int num_hops() {
    return _nhops;
  }
  void set_num_hops(uint8_t n) {
    _nhops = n;
  }
  
  uint16_t get_metric(int h) { 
    uint16_t *ndx = (uint16_t *) (this+1);
    return ndx[h + num_hops()*2];
  }
  
  void set_metric(int hop, uint16_t s) { 
    uint16_t *ndx = (uint16_t *) (this+1);
    ndx[hop + num_hops()*2] = s;
  }
  
  IPAddress get_hop(int h) { 
    in_addr *ndx = (in_addr *) (this + 1);
    return IPAddress(ndx[h]);
  }
  
  void  set_hop(int hop, IPAddress p) { 
    in_addr *ndx = (in_addr *) (this + 1);
    ndx[hop] = p.in_addr();
  }

};

#endif /* CLICK_SRPACKET_HH */
