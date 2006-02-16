#ifndef CLICK_SRPAKCET_HH
#define CLICK_SRPAKCET_HH
#include <click/ipaddress.hh>
#include <elements/wifi/path.hh>
CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))


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

static const uint8_t _sr_version = 0x0b;


// Packet format.
CLICK_SIZE_PACKED_STRUCTURE(
struct srpacket {,
  uint8_t _version; /* see _srcr_version */
  uint8_t _type;  /* see enum SRCRPacketType */
  uint8_t _nlinks;
  uint8_t _next;   // Index of next node who should process this packet.


  uint16_t _ttl;
  uint16_t _cksum;
  uint16_t _flags; 
  uint16_t _dlen;

  /* PT_QUERY
   * _qdst is used for the query destination in control packets
   * and a extra 32 bit seq number in data packets
   */
  uint32_t _qdst;


  uint32_t _seq;   // seq number
  uint32_t _seq2;  // another seq number

  
  /* uin32_t ip[_nlinks] */
  /* uin32_t metrics[_nlinks] */


  /* ip */
  /* fwd */
  /* rev */
  /* seq */
  /* ip */

  uint32_t _random_from;
  uint32_t _random_fwd_metric;
  uint32_t _random_rev_metric;
  uint32_t _random_seq;
  uint16_t _random_age;
  uint32_t _random_to;


  void set_random_from(IPAddress ip) {
    _random_from = ip;
  }
  void set_random_to(IPAddress ip) {
    _random_to = ip;
  }
  void set_random_fwd_metric(uint32_t m) {
    _random_fwd_metric = m;
  }

  void set_random_rev_metric(uint32_t m) {
    _random_rev_metric = m;
  }
  void set_random_seq(uint32_t s) {
    _random_seq = s;
  }
  void set_random_age(uint32_t s) {
    _random_age = s;
  }

  IPAddress get_random_from() {
    return _random_from;
  }
  IPAddress get_random_to() {
    return _random_to;
  }
  uint32_t get_random_fwd_metric() {
    return _random_fwd_metric;
  }
  uint32_t get_random_rev_metric() {
    return _random_rev_metric;
  }

  uint32_t get_random_seq() {
    return _random_seq;
  }

  uint32_t get_random_age() {
    return _random_age;
  }


  void set_link(int link,
		IPAddress a, IPAddress b, 
		uint32_t fwd, uint32_t rev,
		uint32_t seq,
		uint32_t age) {
    
    uint32_t *ndx = (uint32_t *) (this+1);
    ndx += link * 5;

    ndx[0] = a;
    ndx[1] = fwd;
    ndx[2] = rev;
    ndx[3] = seq;
    ndx[4] = age;
    ndx[5] = b;
  }

  uint32_t get_link_fwd(int link) {
    uint32_t *ndx = (uint32_t *) (this+1);
    ndx += link * 5;
    return ndx[1];
  }
  uint32_t get_link_rev(int link) {
    uint32_t *ndx = (uint32_t *) (this+1);
    ndx += link * 5;
    return ndx[2];
  }

  uint32_t get_link_seq(int link) {
    uint32_t *ndx = (uint32_t *) (this+1);
    ndx += link * 5;
    return ndx[3];
  }

  uint32_t get_link_age(int link) {
    uint32_t *ndx = (uint32_t *) (this+1);
    ndx += link * 5;
    return ndx[4];
  }

  IPAddress get_link_node(int link) {
    uint32_t *ndx = (uint32_t *) (this+1);
    ndx += link * 5;
    return ndx[0];
  }


  void set_link_node(int link, IPAddress ip) {
    uint32_t *ndx = (uint32_t *) (this+1);
    ndx += link * 5;
    ndx[0] = ip;
  }




  // How long should the packet be?
  size_t hlen_wo_data() const { return len_wo_data(_nlinks); }
  size_t hlen_with_data() const { return len_with_data(_nlinks, ntohs(_dlen)); }
  
  static size_t len_wo_data(int nlinks) {
    return sizeof(struct srpacket) +
      sizeof(uint32_t) + 
      (nlinks) * sizeof(uint32_t) * 5;

  }
  static size_t len_with_data(int nlinks, int dlen) {
    return len_wo_data(nlinks) + dlen;
  }
  
  int num_links() {
    return _nlinks;
  }

  int next() {
    return _next;
  }
  Path get_path() {
    Path p;
    for (int x = 0; x <= num_links(); x++) {
      p.push_back(get_link_node(x));
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

  void set_num_links(uint8_t n) {
    _nlinks = n;
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


  /* remember that if you call this you must have set the number of links in this packet! */
  u_char *data() { return (((u_char *)this) + len_wo_data(num_links())); }


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
});




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


CLICK_ENDDECLS
#endif /* CLICK_SRPACKET_HH */
