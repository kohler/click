#ifndef PACKET_HH
#define PACKET_HH
#include "ipaddress.hh"
#include "glue.hh"
class IP6Address;
struct click_ip;
struct click_ip6;
class WritablePacket;

class Packet { public:

  // PACKET CREATION
  static unsigned default_headroom()	{ return 28; }
  static unsigned default_tailroom(unsigned len) { return (len<56?64-len:8); }
  
  static WritablePacket *make(unsigned);
  static WritablePacket *make(const char *, unsigned);
  static WritablePacket *make(const unsigned char *, unsigned);
  static WritablePacket *make(unsigned, const unsigned char *, unsigned, unsigned);
  
#ifdef __KERNEL__
  // Packet::make(sk_buff *) wraps a Packet around an existing sk_buff.
  // Packet now owns the sk_buff (ie we don't increment skb->users).
  static Packet *make(struct sk_buff *);
  struct sk_buff *skb() const		{ return (struct sk_buff *)this; }
  struct sk_buff *steal_skb()		{ return skb(); }
  void kill() 				{ kfree_skb(skb()); }
#else
  void kill()				{ if (--_use_count <= 0) delete this; }
#endif

  bool shared() const;
  Packet *clone();
  WritablePacket *uniqueify();
  
#ifdef __KERNEL__
  const unsigned char *data() const	{ return skb()->data; }
  unsigned length() const		{ return skb()->len; }
  unsigned headroom() const		{ return skb()->data - skb()->head; }
  unsigned tailroom() const		{ return skb()->end - skb()->tail; }
  const unsigned char *total_data() const { return skb()->head; }
  unsigned total_length() const		{ return skb()->end - skb()->head; }
#else
  const unsigned char *data() const	{ return _data; }
  unsigned length() const		{ return _tail - _data; }
  unsigned headroom() const		{ return _data - _head; }
  unsigned tailroom() const		{ return _end - _tail; }
  const unsigned char *total_data() const { return _head; }
  unsigned total_length() const		{ return _end - _head; }
#endif
  
  WritablePacket *push(unsigned nb);	// Add more space before packet.
  Packet *nonunique_push(unsigned nb);
  void pull(unsigned nb);		// Get rid of initial bytes.
  WritablePacket *put(unsigned nb);	// Add bytes to end of pkt.
  void take(unsigned nb);		// Delete bytes from end of pkt.

  // HEADER ANNOTATIONS
#ifdef __KERNEL__
  const unsigned char *network_header() const	{ return skb()->nh.raw; }
  const click_ip *ip_header() const	{ return (click_ip *)skb()->nh.iph; }
  const click_ip6 *ip6_header() const	{ return (click_ip6 *)skb()->nh.ipv6h; }
  const unsigned char *transport_header() const	{ return skb()->h.raw; }
#else
  const unsigned char *network_header() const	{ return _nh.raw; }
  const click_ip *ip_header() const		{ return _nh.iph; }
  const click_ip6 *ip6_header() const           { return _nh.ip6h; }
  const unsigned char *transport_header() const	{ return _h_raw; }
#endif
  unsigned network_header_offset() const;
  unsigned network_header_length() const;
  unsigned ip_header_offset() const;
  unsigned ip_header_length() const;
  unsigned ip6_header_length() const;

  void set_network_header(const unsigned char *, unsigned);
  void set_ip_header(const click_ip *, unsigned);
  void set_ip6_header(const click_ip6 *);
  void set_ip6_header(const click_ip6 *, unsigned);
  
  unsigned transport_header_offset() const;

  // ANNOTATIONS

 private:
  class Anno;
#ifdef __KERNEL__
  const Anno *anno() const		{ return (const Anno *)skb()->cb; }
  Anno *anno()				{ return (Anno *)skb()->cb; }
#else
  const Anno *anno() const		{ return (const Anno *)_cb; }
  Anno *anno()				{ return (Anno *)_cb; }
#endif
 public:

  enum PacketType {		// must agree with if_packet.h
    HOST = 0, BROADCAST = 1, MULTICAST = 2, OTHERHOST = 3, OUTGOING = 4,
    LOOPBACK = 5, FASTROUTE = 6
  };

  void copy_annotations(Packet *);
  void clear_annotations();
  
  IPAddress dst_ip_anno() const;
  void set_dst_ip_anno(IPAddress a);
  const IP6Address &dst_ip6_anno() const;
  void set_dst_ip6_anno(const IP6Address &a);

  unsigned char sniff_flags_anno() const{ return anno()->sniff_flags; }
  void set_sniff_flags_anno(unsigned char c)   { anno()->sniff_flags = c; }
#ifdef __KERNEL__
  PacketType packet_type_anno() const	{ return (PacketType)skb()->pkt_type; }
  void set_packet_type_anno(PacketType p) { skb()->pkt_type = p; }
  struct device *device_anno() const	{ return skb()->dev; }
  void set_device_anno(struct device *dev) { skb()->dev = dev; }
#else
  PacketType packet_type_anno() const	{ return _pkt_type; }
  void set_packet_type_anno(PacketType p) { _pkt_type = p; }
  struct device *device_anno() const	{ return 0; }
  void set_device_anno(struct device *) { }
#endif
  bool fix_ip_src_anno() const		{ return anno()->fix_ip_src; }
  void set_fix_ip_src_anno(bool f)	{ anno()->fix_ip_src = f; }
  char param_off_anno() const		{ return anno()->param_off; }
  void set_param_off_anno(char p)	{ anno()->param_off = p; }
  char color_anno() const		{ return anno()->color; }
  void set_color_anno(char c)		{ anno()->color = c; }
  int fwd_rate_anno() const		{ return anno()->fwd_rate; }
  void set_fwd_rate_anno(int r)		{ anno()->fwd_rate = r; }
  int rev_rate_anno() const		{ return anno()->rev_rate; }
  void set_rev_rate_anno(int r)		{ anno()->rev_rate = r; }
#ifdef __KERNEL__
  unsigned long long perfctr_anno() const { return anno()->perfctr; }
  void set_perfctr_anno(unsigned long long pc) { anno()->perfctr = pc; }
#endif
  
 private:
  
  // Anno must fit in sk_buff's char cb[48].
  struct Anno {
    union {
      unsigned dst_ip4;
      unsigned char dst_ip6[16];
    } dst_ip;
    unsigned char sniff_flags; // flags used for sniffers
    bool fix_ip_src : 1;    // flag: asks FixIPSrc to set ip_src
    char param_off;     // for ICMP Parameter Problem, byte offset of error
    char color;         // one of 255 colors set by Paint element
    int fwd_rate;
    int rev_rate;
#ifdef __KERNEL__
    unsigned long long perfctr;
#endif
  };

#ifndef __KERNEL__
  int _use_count;
  Packet *_data_packet;
  /* mimic Linux sk_buff */
  unsigned char *_head; /* start of allocated buffer */
  unsigned char *_data; /* where the packet starts */
  unsigned char *_tail; /* one beyond end of packet */
  unsigned char *_end;  /* one beyond end of allocated buffer */
  unsigned char _cb[48];
  union {
    click_ip *iph;
    click_ip6 *ip6h;
    unsigned char *raw;
  } _nh;
  unsigned char *_h_raw;
  PacketType _pkt_type;
#endif
  
  Packet();
  Packet(const Packet &);
  ~Packet();
  Packet &operator=(const Packet &);

#ifndef __KERNEL__
  Packet(int, int, int)			{ }
  static WritablePacket *make(int, int, int);
  void alloc_data(unsigned, unsigned, unsigned);
#endif

  WritablePacket *uniqueify_copy();
  WritablePacket *expensive_push(unsigned int nbytes);
  WritablePacket *expensive_put(unsigned int nbytes);
  
  friend class WritablePacket;

};


class WritablePacket : public Packet { public:
  
#ifdef __KERNEL__
  unsigned char *data() const			{ return skb()->data; }
  unsigned char *network_header() const		{ return skb()->nh.raw; }
  click_ip *ip_header() const		{ return (click_ip *)skb()->nh.iph; }
  click_ip6 *ip6_header() const         { return (click_ip6*)skb()->nh.ipv6h; }
  unsigned char *transport_header() const	{ return skb()->h.raw; }
#else
  unsigned char *data() const			{ return _data; }
  unsigned char *network_header() const		{ return _nh.raw; }
  click_ip *ip_header() const			{ return _nh.iph; }
  click_ip6 *ip6_header() const                 { return _nh.ip6h; }
  unsigned char *transport_header() const	{ return _h_raw; }
#endif

};


inline WritablePacket *
Packet::make(unsigned len)
{
  return make(default_headroom(), (const unsigned char *)0, len,
	      default_tailroom(len));
}

inline WritablePacket *
Packet::make(const char *s, unsigned len)
{
  return make(default_headroom(), (const unsigned char *)s, len,
	      default_tailroom(len));
}

inline WritablePacket *
Packet::make(const unsigned char *s, unsigned len)
{
  return make(default_headroom(), (const unsigned char *)s, len,
	      default_tailroom(len));
}

#ifdef __KERNEL__
inline Packet *
Packet::make(struct sk_buff *skb)
{
  if (atomic_read(&skb->users) == 1)
    return reinterpret_cast<Packet *>(skb);
  else
    return reinterpret_cast<Packet *>(skb_clone(skb, GFP_ATOMIC));
}
#endif

inline bool
Packet::shared() const
{
#ifdef __KERNEL__
  return skb_cloned(skb());
#else
  return (_data_packet || _use_count > 1);
#endif
}

inline WritablePacket *
Packet::uniqueify()
{
  if (shared())
    return uniqueify_copy();
  else
    return static_cast<WritablePacket *>(this);
}

inline WritablePacket *
Packet::push(unsigned int nbytes)
{
  if (headroom() >= nbytes) {
    WritablePacket *p = uniqueify();
#ifdef __KERNEL__
    __skb_push(p->skb(), nbytes);
#else
    p->_data -= nbytes;
#endif
    return p;
  } else
    return expensive_push(nbytes);
}

inline Packet *
Packet::nonunique_push(unsigned int nbytes)
{
  if (headroom() >= nbytes) {
#ifdef __KERNEL__
    __skb_push(skb(), nbytes);
#else
    _data -= nbytes;
#endif
    return this;
  } else
    return expensive_push(nbytes);
}

/* Get rid of some bytes at the start of a packet */
inline void
Packet::pull(unsigned int nbytes)
{
  if (nbytes > length()) {
    click_chatter("Packet::pull %d > length %d\n", nbytes, length());
    nbytes = length();
  }
#ifdef __KERNEL__
  __skb_pull(skb(), nbytes);
#else
  _data += nbytes;
#endif
}

/* Get rid of some bytes at the end of a packet */
inline void
Packet::take(unsigned int nbytes)
{
  if (nbytes > length()) {
    click_chatter("Packet::take %d > length %d\n", nbytes, length());
    nbytes = length();
  }
#ifdef __KERNEL__
  skb()->tail -= nbytes;
  skb()->len -= nbytes;
#else
  _tail -= nbytes;
#endif    
}

inline const IP6Address &
Packet::dst_ip6_anno() const
{
  return reinterpret_cast<const IP6Address &>(anno()->dst_ip.dst_ip6);
}

inline void
Packet::set_dst_ip6_anno(const IP6Address &a)
{
  memcpy(anno()->dst_ip.dst_ip6, &a, 16);
}

inline IPAddress 
Packet::dst_ip_anno() const
{
  return IPAddress(anno()->dst_ip.dst_ip4);
}

inline void 
Packet::set_dst_ip_anno(IPAddress a)
{ 
  anno()->dst_ip.dst_ip4 = a.addr(); 
}

inline void
Packet::set_network_header(const unsigned char *h, unsigned len)
{
#ifdef __KERNEL__
  skb()->nh.raw = const_cast<unsigned char *>(h);
  skb()->h.raw = const_cast<unsigned char *>(h) + len;
#else
  _nh.raw = const_cast<unsigned char *>(h);
  _h_raw = const_cast<unsigned char *>(h) + len;
#endif
}

inline void
Packet::set_ip_header(const click_ip *iph, unsigned len)
{
  set_network_header(reinterpret_cast<const unsigned char *>(iph), len);
}

inline void
Packet::set_ip6_header(const click_ip6 *ip6h, unsigned len)
{
  set_network_header(reinterpret_cast<const unsigned char *>(ip6h), len);
}

inline void
Packet::set_ip6_header(const click_ip6 *ip6h)
{ 
  set_ip6_header(ip6h, 40);
}

inline unsigned
Packet::network_header_offset() const
{
  return network_header() - data();
}

inline unsigned
Packet::network_header_length() const
{
  return transport_header() - network_header();
}

inline unsigned
Packet::ip_header_offset() const
{
  return network_header_offset();
}

inline unsigned
Packet::ip_header_length() const
{
  return network_header_length();
}

inline unsigned
Packet::ip6_header_length() const
{
  return network_header_length();
}

inline unsigned
Packet::transport_header_offset() const
{
  return transport_header() - data();
}

inline void
Packet::copy_annotations(Packet *p)
{
  *anno() = *p->anno();
}

inline void
Packet::clear_annotations()
{
  memset(anno(), '\0', sizeof(Anno));
  set_packet_type_anno(HOST);
  set_device_anno(0);
  set_network_header(0, 0);
}

#endif
