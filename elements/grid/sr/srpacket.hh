#ifndef CLICK_SRPAKCET_HH
#define CLICK_SRPAKCET_HH
#include <click/ipaddress.hh>
#include <elements/grid/sr/path.hh>

enum SRCRPacketType { PT_QUERY = 0x01,
		      PT_REPLY = 0x02,
                      PT_TOP5_RESULT = 0x03,
		      PT_DATA  = 0x04,
                      PT_GATEWAY = 0x08
};



enum SRCRPacketFlags {
  FLAG_ERROR = (1<<0),
  FLAG_UPDATE = (1<<1),
  FLAG_TOP5_REQUEST_RESULT = (1<<2),
  FLAG_TOP5_BEST_ROUTE = (1<<3),
  FLAG_SCHEDULE = (1<<4),
  FLAG_SCHEDULE_TOKEN = (1<<5),
  FLAG_SCHEDULE_FAKE = (1<<6),
  FLAG_ECN = (1<<7)
};

static const uint8_t _sr_version = 0x08;

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

  uint16_t _random_fwd_metric;
  uint16_t _random_rev_metric;

  /* PT_QUERY
   * _qdst is used for the query destination in control packets
   * and a extra 32 bit seq number in data packets
   */
  uint32_t _qdst;


  uint32_t _random_from;
  uint32_t _random_to;


  uint32_t _seq;   // seq number
  uint32_t _seq2;  // another seq number

  
  


  void set_random_from(IPAddress ip) {
    _random_from = ip;
  }
  void set_random_to(IPAddress ip) {
    _random_to = ip;
  }
  void set_random_fwd_metric(uint16_t m) {
    _random_fwd_metric = m;
  }

  void set_random_rev_metric(uint16_t m) {
    _random_rev_metric = m;
  }
  IPAddress get_random_from() {
    return _random_from;
  }
  IPAddress get_random_to() {
    return _random_to;
  }
  int get_random_fwd_metric() {
    return _random_fwd_metric;
  }
  int get_random_rev_metric() {
    return _random_rev_metric;
  }


  // How long should the packet be?
  size_t hlen_wo_data() const { return len_wo_data(_nhops); }
  size_t hlen_with_data() const { return len_with_data(_nhops, ntohs(_dlen)); }
  
  static size_t len_wo_data(int nhops) {
    return sizeof(struct srpacket) 
      + nhops * sizeof(uint32_t)        // each ip address
      + 2 * (nhops) * sizeof(uint16_t); //metrics in both directions
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
  Path get_path() {
    Path p;
    for (int x = 0; x < num_hops(); x++) {
      p.push_back(get_hop(x));
    }
    return p;
  }
  void set_data_seq(uint32_t n) {
    _qdst = htonl(n);
  }
  uint32_t data_seq() {
    return ntohl(_qdst);
  }
  void set_seq(uint32_t n) {
    _seq = htonl(n);
  }
  uint32_t seq() {
    return ntohl(_seq);
  }

  void set_seq2(uint32_t n) {
    _seq2 = htonl(n);
  }
  uint32_t seq2() {
    return ntohl(_seq2);
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


  void set_flag(uint16_t f) {
    uint16_t flags = ntohs(_flags);
    _flags = htons(flags | f);
  }
  
  bool flag(int f) {
    int x = ntohs(_flags);
    return x & f;
  }
  void unset_flag(uint16_t f) {
    uint16_t flags = ntohs(_flags);
    _flags = htons(flags & !f);
  }

  uint16_t get_fwd_metric(int h) { 
    uint16_t *ndx = (uint16_t *) (this+1);
    return ndx[2*h + num_hops()*2];
  }

  uint16_t get_rev_metric(int h) { 
    uint16_t *ndx = (uint16_t *) (this+1);
    return ndx[1 + 2*h  + num_hops()*2];
  }

  void set_fwd_metric(int hop, uint16_t s) { 
    uint16_t *ndx = (uint16_t *) (this+1);
    ndx[2*hop + num_hops()*2] = s;
  }


  void set_rev_metric(int hop, uint16_t s) { 
    uint16_t *ndx = (uint16_t *) (this+1);
    ndx[1 + 2*hop + num_hops()*2] = s;
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

  void set_checksum() {
    unsigned int tlen = 0;
    if (_type & PT_DATA) {
      tlen = hlen_with_data();
    } else {
      tlen = hlen_wo_data();
    }
    _cksum = 0;
    _cksum = click_in_cksum((unsigned char *) this, tlen);
  }
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

#ifndef sr_assert
#define sr_assert(e) ((e) ? (void) 0 : sr_assert_(__FILE__, __LINE__, #e))
#endif /* sr_assert */


inline void 
sr_assert_(const char *file, int line, const char *expr)
{
  click_chatter("assertion \"%s\" FAILED: file %s, line %d",
		expr, file, line);

#ifdef CLICK_USERLEVEL  
  abort();
#endif

}



#endif /* CLICK_SRPACKET_HH */
