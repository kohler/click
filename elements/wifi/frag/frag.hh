#ifndef CLICK_FRAG_HH
#define CLICK_FRAG_HH

CLICK_DECLS

CLICK_SIZE_PACKED_STRUCTURE(
struct frag {,
  uint16_t packet_num;
  uint8_t frag_num;
  uint8_t frag_num2;
  uint16_t checksum;
  u_char *data() { return ((u_char *) (this + 1)); }
  void set_checksum(int frag_size) {
    checksum = 0;
    checksum = click_in_cksum((unsigned char *) this, 
			      sizeof(struct frag) + frag_size);
  }

  bool valid_checksum(int frag_size) {
    return click_in_cksum((unsigned char *) this, 
			  sizeof(struct frag) + frag_size) == 0;
  }
});

enum FragFlags {
  FRAG_ACKME = (1<<0),
  FRAG_RESEND = (1<<1),
};

struct frag_header {
  /* normal ether header */
  uint8_t dst[6];
  uint8_t src[6];
  uint16_t ether_type;
  /* rest of header */
  uint16_t flags;
  uint16_t num_frags; /* frags in *this* packet */
  uint16_t num_frags_packet;
  uint16_t packet_num;
  uint16_t frag_size;
  uint16_t header_checksum;
  struct frag *get_frag(int x) {
    return (struct frag *) (((char *)(this+1) + 
			     x*(sizeof(struct frag)+frag_size)));
  }

  static size_t packet_size(int frags, int frag_size) {
    return sizeof(frag_header) + frags*(frag_size+sizeof(frag));
  }

  void set_checksum() {
    header_checksum = 0;
    header_checksum = click_in_cksum((uint8_t *)this, 
			      sizeof(struct frag_header));
  }

  bool valid_checksum() {
    return click_in_cksum((uint8_t *) this, 
			  sizeof(struct frag_header)) == 0;
  }
};

struct frag_ack {
  uint16_t good_until;
  uint16_t num_acked;

  uint16_t get_packet(int x) {
    uint16_t *ndx = (uint16_t *) (this+1);
    return ndx[x];
  }

  void  set_packet(int x, uint16_t n) {
    uint16_t *ndx = (uint16_t *) (this+1);
    ndx[x] = n;
  }

  uint8_t get_frag(int x) {
    uint8_t *ndx = (uint8_t *) (this+1);
    return ndx[2*num_acked +  x];
  }

  void  set_frag(int x, uint8_t n) {
    uint8_t *ndx = (uint8_t *) (this+1);
    ndx[2*num_acked +  x] = n;
  }

  static size_t packet_size(int acks) {
    return sizeof(click_ether) + sizeof(frag_ack) + acks*(sizeof(uint16_t) + sizeof(uint8_t));
  }
  
};



struct fragid {
  int packet_num;
  int frag_num;
  fragid(int p, int f) {
    packet_num = p;
    frag_num = f;
  }

  fragid(struct frag *f) {
    packet_num = f->packet_num;
    frag_num = f->frag_num;
  }

  fragid(const struct fragid &f) {
    packet_num = f.packet_num;
    frag_num = f.frag_num;
  }

  inline bool
  operator==(fragid other)
  {
    return (other.packet_num == packet_num && other.frag_num == frag_num);
  }

  inline bool
  operator<(fragid other)
  {
    if (valid() != other.valid()) {
      return !valid();
    }

    if (other.packet_num == packet_num) {
      return other.frag_num < frag_num;
    } 
    return other.packet_num < packet_num;
  }

  inline bool
  operator>(fragid other) {
    return !operator<(other);
  }
  bool valid() {
    return (packet_num != -1 && frag_num != -1);
  }
  void mark_invalid() {
    packet_num = frag_num = -1;
  }

  String s() {
    return String(packet_num) + " "  + String(frag_num);
  }
};



CLICK_ENDDECLS
#endif /* CLICK_FRAG_HH */
