#ifndef PACKET_HH
#define PACKET_HH
#include <click/ipaddress.hh>
#include <click/glue.hh>
#ifdef CLICK_LINUXMODULE
# include <click/skbmgr.hh>
#endif

class IP6Address;
struct click_ip;
struct click_ip6;
class WritablePacket;

class Packet { public:

  // PACKET CREATION
  static const unsigned DEFAULT_HEADROOM = 28;
  static const unsigned MIN_BUFFER_LENGTH = 64;
  
  static WritablePacket *make(unsigned);
  static WritablePacket *make(const char *, unsigned);
  static WritablePacket *make(const unsigned char *, unsigned);
  static WritablePacket *make(unsigned, const unsigned char *, unsigned, unsigned);
  
#ifdef __KERNEL__
  // Packet::make(sk_buff *) wraps a Packet around an existing sk_buff.
  // Packet now owns the sk_buff (ie we don't increment skb->users).
  static Packet *make(struct sk_buff *);
  struct sk_buff *skb()			{ return (struct sk_buff *)this; }
  const struct sk_buff *skb() const	{ return (const struct sk_buff*)this; }
  struct sk_buff *steal_skb()		{ return skb(); }
  void kill();
#else
  static WritablePacket *make(unsigned char *, unsigned, void (*destructor)(unsigned char *, size_t));
  void kill()				{ if (--_use_count <= 0) delete this; }
#endif

  bool shared() const;
  Packet *clone();
  WritablePacket *uniqueify();
#ifndef __KERNEL__
  int use_count() const			{ return _use_count; }
#endif
  
#ifdef __KERNEL__
  const unsigned char *data() const	{ return skb()->data; }
  unsigned length() const		{ return skb()->len; }
  unsigned headroom() const		{ return skb()->data - skb()->head; }
  unsigned tailroom() const		{ return skb()->end - skb()->tail; }
  const unsigned char *buffer_data() const { return skb()->head; }
  unsigned buffer_length() const	{ return skb()->end - skb()->head; }
#else
  const unsigned char *data() const	{ return _data; }
  unsigned length() const		{ return _tail - _data; }
  unsigned headroom() const		{ return _data - _head; }
  unsigned tailroom() const		{ return _end - _tail; }
  const unsigned char *buffer_data() const { return _head; }
  unsigned buffer_length() const	{ return _end - _head; }
#endif
  
  WritablePacket *push(unsigned nb);	// Add more space before packet.
  Packet *nonunique_push(unsigned nb);
  void pull(unsigned nb);		// Get rid of initial bytes.
  WritablePacket *put(unsigned nb);	// Add bytes to end of pkt.
  Packet *nonunique_put(unsigned nb);
  void take(unsigned nb);		// Delete bytes from end of pkt.

#ifndef __KERNEL__
  void change_headroom_and_length(unsigned headroom, unsigned length);
#endif

  // HEADER ANNOTATIONS
#ifdef __KERNEL__
  const unsigned char *network_header() const	{ return skb()->nh.raw; }
  const click_ip *ip_header() const	{ return (click_ip *)skb()->nh.iph; }
  const click_ip6 *ip6_header() const	{ return (click_ip6 *)skb()->nh.ipv6h; }
#else
  const unsigned char *network_header() const	{ return _nh.raw; }
  const click_ip *ip_header() const		{ return _nh.iph; }
  const click_ip6 *ip6_header() const           { return _nh.ip6h; }
#endif
  int network_header_offset() const;
  unsigned network_header_length() const;
  int ip_header_offset() const;
  unsigned ip_header_length() const;
  int ip6_header_offset() const;
  unsigned ip6_header_length() const;

#ifdef __KERNEL__
  const unsigned char *transport_header() const	{ return skb()->h.raw; }
#else
  const unsigned char *transport_header() const	{ return _h_raw; }
#endif
  unsigned transport_header_offset() const;

  void set_network_header(const unsigned char *, unsigned);
  void set_ip_header(const click_ip *, unsigned);
  void set_ip6_header(const click_ip6 *);
  void set_ip6_header(const click_ip6 *, unsigned);
  
  // ANNOTATIONS

 private:
  struct Anno;
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

  IPAddress dst_ip_anno() const;
  void set_dst_ip_anno(IPAddress);
  const IP6Address &dst_ip6_anno() const;
  void set_dst_ip6_anno(const IP6Address &);

#ifdef __KERNEL__
  const struct timeval &timestamp_anno() const { return skb()->stamp; }
  struct timeval &timestamp_anno()	{ return skb()->stamp; }
  void set_timestamp_anno(const struct timeval &tv) { skb()->stamp = tv; }
  void set_timestamp_anno(int s, int us) { skb()->stamp.tv_sec = s; skb()->stamp.tv_usec = us; }
  net_device *device_anno() const	{ return skb()->dev; }
  void set_device_anno(net_device *dev)	{ skb()->dev = dev; }
  PacketType packet_type_anno() const	{ return (PacketType)(skb()->pkt_type & PACKET_TYPE_MASK); }
  void set_packet_type_anno(PacketType p) { skb()->pkt_type = (skb()->pkt_type & PACKET_CLEAN) | p; }
# ifdef HAVE_INT64_TYPES
  uint64_t perfctr_anno() const		{ return anno()->perfctr; }
  void set_perfctr_anno(uint64_t pc)	{ anno()->perfctr = pc; }
# endif
#else
  const struct timeval &timestamp_anno() const { return _timestamp; }
  struct timeval &timestamp_anno()	{ return _timestamp; }
  void set_timestamp_anno(const struct timeval &tv) { _timestamp = tv; }
  void set_timestamp_anno(int s, int us) { _timestamp.tv_sec = s; _timestamp.tv_usec = us; }
  net_device *device_anno() const	{ return 0; }
  void set_device_anno(net_device *)	{ }
  PacketType packet_type_anno() const	{ return _pkt_type; }
  void set_packet_type_anno(PacketType p) { _pkt_type = p; }
#endif

  static const int USER_ANNO_SIZE = 12;
  static const int USER_ANNO_U_SIZE = 3;
  static const int USER_ANNO_I_SIZE = 3;
  
  unsigned char user_anno_c(int i) const { return anno()->user_flags.c[i]; }
  void set_user_anno_c(int i, unsigned char v) { anno()->user_flags.c[i] = v; }
  unsigned user_anno_u(int i) const	{ return anno()->user_flags.u[i]; }
  void set_user_anno_u(int i, unsigned v) { anno()->user_flags.u[i] = v; }
  unsigned *all_user_anno_u()		{ return &anno()->user_flags.u[0]; }
  int user_anno_i(int i) const		{ return anno()->user_flags.i[i]; }
  void set_user_anno_i(int i, int v)	{ anno()->user_flags.i[i] = v; }

  void clear_annotations();
  void copy_annotations(const Packet *);
  
 private:

  // Anno must fit in sk_buff's char cb[48].
  struct Anno {
    union {
      unsigned dst_ip4;
      unsigned char dst_ip6[16];
    } dst_ip;
    
    union {
      unsigned char c[USER_ANNO_SIZE];
      unsigned u[USER_ANNO_U_SIZE];
      int i[USER_ANNO_I_SIZE];
    } user_flags;
    // flag allocations: see packet_anno.hh
    
#if defined(__KERNEL__) && defined(HAVE_INT64_TYPES)
    uint64_t perfctr;
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
  void (*_destructor)(unsigned char *, size_t);
  unsigned char _cb[48];
  union {
    click_ip *iph;
    click_ip6 *ip6h;
    unsigned char *raw;
  } _nh;
  unsigned char *_h_raw;
  PacketType _pkt_type;
  struct timeval _timestamp;
#endif
  
  Packet();
  Packet(const Packet &);
  ~Packet();
  Packet &operator=(const Packet &);

#ifndef __KERNEL__
  Packet(int, int, int)			{ }
  static WritablePacket *make(int, int, int);
  bool alloc_data(unsigned, unsigned, unsigned);
#endif

  WritablePacket *expensive_uniqueify();
  WritablePacket *expensive_push(unsigned int nbytes);
  WritablePacket *expensive_put(unsigned int nbytes);
  
  friend class WritablePacket;

};


class WritablePacket : public Packet { public:
  
#ifdef __KERNEL__
  unsigned char *data() const			{ return skb()->data; }
  unsigned char *buffer_data() const		{ return skb()->head; }
  unsigned char *network_header() const		{ return skb()->nh.raw; }
  click_ip *ip_header() const		{ return (click_ip *)skb()->nh.iph; }
  click_ip6 *ip6_header() const         { return (click_ip6*)skb()->nh.ipv6h; }
  unsigned char *transport_header() const	{ return skb()->h.raw; }
#else
  unsigned char *data() const			{ return _data; }
  unsigned char *buffer_data() const		{ return _head; }
  unsigned char *network_header() const		{ return _nh.raw; }
  click_ip *ip_header() const			{ return _nh.iph; }
  click_ip6 *ip6_header() const                 { return _nh.ip6h; }
  unsigned char *transport_header() const	{ return _h_raw; }
#endif

 private:

  WritablePacket()				{ }
  WritablePacket(const Packet &)		{ }
  ~WritablePacket()				{ }

  friend class Packet;
  
};


inline WritablePacket *
Packet::make(unsigned len)
{
  return make(DEFAULT_HEADROOM, (const unsigned char *)0, len, 0);
}

inline WritablePacket *
Packet::make(const char *s, unsigned len)
{
  return make(DEFAULT_HEADROOM, (const unsigned char *)s, len, 0);
}

inline WritablePacket *
Packet::make(const unsigned char *s, unsigned len)
{
  return make(DEFAULT_HEADROOM, (const unsigned char *)s, len, 0);
}

#ifdef __KERNEL__
inline Packet *
Packet::make(struct sk_buff *skb)
{
  if (atomic_read(&skb->users) == 1)
    return reinterpret_cast<Packet *>(skb);
  else {
    Packet *p = reinterpret_cast<Packet *>(skb_clone(skb, GFP_ATOMIC));
    atomic_dec(&skb->users);
    return p;
  }
}

inline void
Packet::kill()
{
  struct sk_buff *b = skb();
  b->next = b->prev = 0;
  b->list = 0;
  skbmgr_recycle_skbs(b, 1);
}
#endif

inline bool
Packet::shared() const
{
#ifdef __KERNEL__
  return skb_cloned(const_cast<struct sk_buff *>(skb()));
#else
  return (_data_packet || _use_count > 1);
#endif
}

inline WritablePacket *
Packet::uniqueify()
{
  if (!shared())
    return static_cast<WritablePacket *>(this);
  else
    return expensive_uniqueify();
}

inline WritablePacket *
Packet::push(unsigned int nbytes)
{
  if (headroom() >= nbytes && !shared()) {
    WritablePacket *q = (WritablePacket *)this;
#ifdef __KERNEL__
    __skb_push(q->skb(), nbytes);
#else
    q->_data -= nbytes;
#endif
    return q;
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

inline WritablePacket *
Packet::put(unsigned int nbytes)
{
  if (tailroom() >= nbytes && !shared()) {
    WritablePacket *q = (WritablePacket *)this;
#ifdef __KERNEL__
    __skb_put(q->skb(), nbytes);
#else
    q->_tail += nbytes;
#endif
    return q;
  } else
    return expensive_put(nbytes);
}

inline Packet *
Packet::nonunique_put(unsigned int nbytes)
{
  if (tailroom() >= nbytes) {
#ifdef __KERNEL__
    __skb_put(skb(), nbytes);
#else
    _tail += nbytes;
#endif
    return this;
  } else
    return expensive_put(nbytes);
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

#ifndef __KERNEL__
inline void
Packet::change_headroom_and_length(unsigned headroom, unsigned length)
{
  if (headroom + length <= buffer_length()) {
    _data = _head + headroom;
    _tail = _data + length;
  }
}
#endif

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

inline int
Packet::network_header_offset() const
{
  return network_header() - data();
}

inline unsigned
Packet::network_header_length() const
{
  return transport_header() - network_header();
}

inline int
Packet::ip_header_offset() const
{
  return network_header_offset();
}

inline unsigned
Packet::ip_header_length() const
{
  return network_header_length();
}

inline int
Packet::ip6_header_offset() const
{
  return network_header_offset();
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
Packet::clear_annotations()
{
  memset(anno(), '\0', sizeof(Anno));
  set_packet_type_anno(HOST);
  set_device_anno(0);
  set_timestamp_anno(0, 0);
  set_network_header(0, 0);
}

inline void
Packet::copy_annotations(const Packet *p)
{
  *anno() = *p->anno();
  set_packet_type_anno(p->packet_type_anno());
  set_device_anno(p->device_anno());
  set_timestamp_anno(p->timestamp_anno());
}

#endif
