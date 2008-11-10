#ifndef CLICK_DSR_H
#define CLICK_DSR_H

/* #include <netinet/in.h> */

CLICK_DECLS

struct click_dsr { // DSR options header -- exactly one per packet
  unsigned char dsr_next_header; // original IP protocol field
  unsigned char dsr_reserved;
  u_int16_t dsr_len;
};

struct click_dsr_option { // common part of option headers, used for determining type
  unsigned char dsr_type;
  unsigned char dsr_len;
};

struct DSRHop {
  in_addr _ip;
  unsigned char _metric;

#define DSR_INVALID_HOP_METRIC 0xFF
#define DSR_INVALID_ROUTE_METRIC 9999

  DSRHop(IPAddress i) : _ip(i), _metric(DSR_INVALID_HOP_METRIC) {}
  DSRHop(IPAddress i, unsigned char c) : _ip(i), _metric(c) {}
  IPAddress ip() const { return IPAddress(_ip); }
};

typedef Vector<DSRHop> DSRRoute;

struct click_dsr_rreq {
  unsigned char dsr_type;
  unsigned char dsr_len;
  u_int16_t dsr_id;

  in_addr target;
  DSRHop addr[];

  unsigned int num_addrs() const {
    if ((dsr_len - 6) % sizeof(DSRHop) != 0) click_chatter("click_dsr_rreq::length() -- warning: odd length (%d %d)\n",
							   dsr_len, sizeof(DSRHop));
    return ((dsr_len - 6)/sizeof(DSRHop));
  }
  unsigned int length() const { return (dsr_len + 2); }
  char *next_option() const { return ((char *)this + length()); }
};

struct click_dsr_rrep {
  unsigned char dsr_type;
  unsigned char dsr_len;
  unsigned char dsr_flags;

  unsigned char dsr_pad; // for alignment; against the spec

  //  unsigned short dsr_id; // this doesn't exist in the spec
  DSRHop addr[];

  unsigned int num_addrs() const {
    if ((dsr_len - 1) % sizeof(DSRHop) != 0) click_chatter("click_dsr_rrep::length() -- warning: odd length\n");
    return ((dsr_len - 1)/sizeof(DSRHop));   // from the spec, sec 6.3
  }
  unsigned int length() const { return (dsr_len + 3); }
  char *next_option() const { return ((char *)this + length()); }
};

struct click_dsr_rerr {
  unsigned char dsr_type;
  unsigned char dsr_len;
  unsigned char dsr_error;

// #define DSR_RERR_FLAGS_SALVAGE_MASK     0x0F  // 00001111
#define DSR_RERR_TYPE_NODE_UNREACHABLE  1

  unsigned char dsr_flags;

  in_addr dsr_err_src;
  in_addr dsr_err_dst;
  // type-specific information follows here

  unsigned int length() const {
    //    if (dsr_error == DSR_RERR_TYPE_NODE_UNREACHABLE) return (dsr_len + 2 + 4);
    //      click_chatter("click_dsr_rerr::length() -- warning: unknown err type %x\n", dsr_error);
    return (dsr_len + 2);
  }
  char *next_option() const { return ((char *)this + length()); }
};

// #define DSR_SR_FLAGS_FIRSTHOPEXT_MASK 0x8000  // 1000000000000000
// #define DSR_SR_FLAGS_LASTHOPEXT_MASK  0x4000  // 0100000000000000
// #define DSR_SR_FLAGS_SALVAGE_MASK     0x03C0  // 0000001111000000
// #define DSR_SR_FLAGS_SEGSLEFT_MASK    0x003F  // 0000000000111111

struct click_dsr_source {
  unsigned char dsr_type;
  unsigned char dsr_len;
  //  u_int16_t dsr_flags;
  unsigned char dsr_salvage;
  unsigned char dsr_segsleft;

  DSRHop addr[];

  unsigned int num_addrs() const {
    if ((dsr_len - 2) % sizeof(DSRHop) != 0) click_chatter("click_dsr_source::length() -- warning: odd length\n");
    return ((dsr_len - 2) / sizeof(DSRHop));
  }
  unsigned int length() const { return (dsr_len + 2); }
  char *next_option() const { return ((char *)this + length()); }
};

// constants the guy doesn't use

/* Constants used - need to check their values */
// static const int DSR_RREQ_RETRIES = 2;
// static const int DSR_NODE_TRAVERSAL_MS = 40;
// static const int DSR_NET_DIAMETER = 35;
// static const int DSR_NET_TRAVERSAL_MS = 3 * DSR_NODE_TRAVERSAL_MS * DSR_NET_DIAMETER / 2;
// static const int DSR_RREQ_START_TTL = 2;
// static const int DSR_RREQ_INCREMENT_TTL = 2;
// static const int DSR_RREQ_THRESHOLD_TTL = 7;
// static const int DSR_BROADCAST_RECORD_MS = 2 * DSR_NET_TRAVERSAL_MS;

#define DSR_TYPE_RREP         1
#define DSR_TYPE_RREQ         2
#define DSR_TYPE_RERR         3
#define DSR_TYPE_SOURCE_ROUTE 96

// #define DSR_TYPE_ACK          32
// #define DSR_TYPE_AREQ         160

#define IP_PROTO_DSR          200

#define DSR_SALVAGE_LIMIT     4

#define DSR_LAST_HOP_IP_ANNO(p)		((p)->anno_u32(Packet::user_anno_offset + 4))
#define SET_DSR_LAST_HOP_IP_ANNO(p, v)	((p)->set_anno_u32(Packet::user_anno_offset + 4, (v)))

#define DSR_LAST_HOP_ETH_ANNO1(p)	((p)->anno_u16(Packet::user_anno_offset + 18))
#define SET_DSR_LAST_HOP_ETH_ANNO1(p, v) ((p)->set_anno_u16(Packet::user_anno_offset + 18, (v)))

#define DSR_LAST_HOP_ETH_ANNO2(p)	((p)->anno_u16(Packet::user_anno_offset + 22))
#define SET_DSR_LAST_HOP_ETH_ANNO2(p, v) ((p)->set_anno_u16(Packet::user_anno_offset + 22, (v)))

#define DSR_LAST_HOP_ETH_ANNO3(p)	((p)->anno_u16(Packet::user_anno_offset + 26))
#define SET_DSR_LAST_HOP_ETH_ANNO3(p, v) ((p)->set_anno_u16(Packet::user_anno_offset + 26, (v)))

CLICK_ENDDECLS

#endif
