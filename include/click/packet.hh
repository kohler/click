// -*- c-basic-offset: 2; related-file-name: "../../lib/packet.cc" -*-
#ifndef CLICK_PACKET_HH
#define CLICK_PACKET_HH
#include <click/ipaddress.hh>
#include <click/glue.hh>
#ifdef CLICK_LINUXMODULE
# include <click/skbmgr.hh>
#endif
struct click_ether;
struct click_ip;
struct click_ip6;
struct click_tcp;
struct click_udp;
CLICK_DECLS

class IP6Address;
class WritablePacket;

class Packet { public:

  // PACKET CREATION
  enum { DEFAULT_HEADROOM = 28, MIN_BUFFER_LENGTH = 64 };
  
  static WritablePacket *make(uint32_t);
  static WritablePacket *make(const char *, uint32_t);
  static WritablePacket *make(const unsigned char *, uint32_t);
  static WritablePacket *make(uint32_t, const unsigned char *, uint32_t, uint32_t);
  
#ifdef CLICK_LINUXMODULE
  // Packet::make(sk_buff *) wraps a Packet around an existing sk_buff.
  // Packet now owns the sk_buff (ie we don't increment skb->users).
  static Packet *make(struct sk_buff *);
  struct sk_buff *skb()			{ return (struct sk_buff *)this; }
  const struct sk_buff *skb() const	{ return (const struct sk_buff*)this; }
  struct sk_buff *steal_skb()		{ return skb(); }
  void kill();
#elif CLICK_BSDMODULE
  // Packet::make(mbuf *) wraps a Packet around an existing mbuf.
  // Packet now owns the mbuf.
  static Packet *make(struct mbuf *);
  struct mbuf *m()			{ return _m; }
  const struct mbuf *m() const		{ return (const struct mbuf *)_m; }
  struct mbuf *steal_m();
  void kill()				{ if (--_use_count <= 0) delete this; }
#else			/* User-space */
  static WritablePacket *make(unsigned char *, uint32_t, void (*destructor)(unsigned char *, size_t));
  void kill()				{ if (--_use_count <= 0) delete this; }
#endif

  bool shared() const;
  Packet *clone();
  WritablePacket *uniqueify();
  
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  const unsigned char *data() const	{ return skb()->data; }
  uint32_t length() const		{ return skb()->len; }
  uint32_t headroom() const		{ return skb()->data - skb()->head; }
  uint32_t tailroom() const		{ return skb()->end - skb()->tail; }
  const unsigned char *buffer_data() const { return skb()->head; }
  uint32_t buffer_length() const	{ return skb()->end - skb()->head; }
#else				/* User-level driver and BSD kernel module */
  const unsigned char *data() const	{ return _data; }
  uint32_t length() const		{ return _tail - _data; }
  uint32_t headroom() const		{ return _data - _head; }
  uint32_t tailroom() const		{ return _end - _tail; }
  const unsigned char *buffer_data() const { return _head; }
  uint32_t buffer_length() const	{ return _end - _head; }
#endif
  
  WritablePacket *push(uint32_t nb);	// Add more space before packet.
  WritablePacket *push_mac_header(uint32_t nb);
  Packet *nonunique_push(uint32_t nb);
  void pull(uint32_t nb);		// Get rid of initial bytes.
  WritablePacket *put(uint32_t nb);	// Add bytes to end of pkt.
  Packet *nonunique_put(uint32_t nb);
  void take(uint32_t nb);		// Delete bytes from end of pkt.

  Packet *shift_data(int offset, bool free_on_failure = true);
#ifdef CLICK_USERLEVEL
  void shrink_data(const unsigned char *, uint32_t length);
#endif

  // HEADER ANNOTATIONS
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  const unsigned char *mac_header() const	{ return skb()->mac.raw; }
  const click_ether *ether_header() const	{ return (click_ether *)skb()->mac.ethernet; }
  
  const unsigned char *network_header() const	{ return skb()->nh.raw; }
  const click_ip *ip_header() const	{ return (click_ip *)skb()->nh.iph; }
  const click_ip6 *ip6_header() const	{ return (click_ip6 *)skb()->nh.ipv6h; }

  const unsigned char *transport_header() const	{ return skb()->h.raw; }
  const click_tcp *tcp_header() const	{ return (click_tcp *)skb()->h.th; }
  const click_udp *udp_header() const	{ return (click_udp *)skb()->h.uh; }
#else			/* User space and BSD kernel module */
  const unsigned char *mac_header() const	{ return _mac.raw; }
  const click_ether *ether_header() const	{ return _mac.ethernet; }

  const unsigned char *network_header() const	{ return _nh.raw; }
  const click_ip *ip_header() const		{ return _nh.iph; }
  const click_ip6 *ip6_header() const           { return _nh.ip6h; }

  const unsigned char *transport_header() const	{ return _h.raw; }
  const click_tcp *tcp_header() const	{ return _h.th; }
  const click_udp *udp_header() const	{ return _h.uh; }
#endif

  void set_mac_header(const unsigned char *);
  void set_mac_header(const unsigned char *, uint32_t);
  void set_ether_header(const click_ether *);
  void set_network_header(const unsigned char *, uint32_t);
  void set_network_header_length(uint32_t);
  void set_ip_header(const click_ip *, uint32_t);
  void set_ip6_header(const click_ip6 *);
  void set_ip6_header(const click_ip6 *, uint32_t);

  int mac_header_offset() const;
  uint32_t mac_header_length() const;

  int network_header_offset() const;
  uint32_t network_header_length() const;
  int ip_header_offset() const;
  uint32_t ip_header_length() const;
  int ip6_header_offset() const;
  uint32_t ip6_header_length() const;

  int transport_header_offset() const;

#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  int mac_length() const		{ return skb()->tail - skb()->mac.raw;}
  int network_length() const		{ return skb()->tail - skb()->nh.raw; }
  int transport_length() const		{ return skb()->tail - skb()->h.raw; }
#else				/* User space and BSD kernel module */
  int mac_length() const		{ return _tail - _mac.raw; }
  int network_length() const		{ return _tail - _nh.raw; }
  int transport_length() const		{ return _tail - _h.raw; }
#endif

  // LINKS
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  Packet *next() const			{ return (Packet *)(skb()->next); }
  Packet *&next()			{ return (Packet *&)(skb()->next); }
  void set_next(Packet *p)		{ skb()->next = p->skb(); }
#else				/* User space and BSD kernel module */ 
  Packet *next() const			{ return _next; }
  Packet *&next()			{ return _next; }
  void set_next(Packet *p)		{ _next = p; }
#endif
  
  // ANNOTATIONS

 private:
  struct Anno;
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  const Anno *anno() const		{ return (const Anno *)skb()->cb; }
  Anno *anno()				{ return (Anno *)skb()->cb; }
#else				/* User-space and BSD kernel module */
  const Anno *anno() const		{ return (const Anno *)_cb; }
  Anno *anno()				{ return (Anno *)_cb; }
#endif
 public:

  enum PacketType {		// must agree with if_packet.h
    HOST = 0, BROADCAST = 1, MULTICAST = 2, OTHERHOST = 3, OUTGOING = 4,
    LOOPBACK = 5, FASTROUTE = 6
  };

  enum { ADDR_ANNO_SIZE = 16 };

  uint8_t *addr_anno()			{ return anno()->addr.c; }
  const uint8_t *addr_anno() const	{ return anno()->addr.c; }
  IPAddress dst_ip_anno() const;
  void set_dst_ip_anno(IPAddress);
  const IP6Address &dst_ip6_anno() const;
  void set_dst_ip6_anno(const IP6Address &);

#ifdef CLICK_LINUXMODULE
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

#else			/* User-space and BSD kernel module */

  const struct timeval &timestamp_anno() const { return _timestamp; }
  struct timeval &timestamp_anno()	{ return _timestamp; }
  void set_timestamp_anno(const struct timeval &tv) { _timestamp = tv; }
  void set_timestamp_anno(int s, int us) { _timestamp.tv_sec = s; _timestamp.tv_usec = us; }
# ifdef CLICK_BSDMODULE	/* BSD kernel module */
  net_device *device_anno() const	{ if (m()) return m()->m_pkthdr.rcvif;
					  else return NULL; }
  void set_device_anno(net_device *dev)	{ if (m()) m()->m_pkthdr.rcvif = dev; }
# else
  net_device *device_anno() const	{ return 0; }
  void set_device_anno(net_device *)	{ }
# endif
  PacketType packet_type_anno() const	{ return _pkt_type; }
  void set_packet_type_anno(PacketType p) { _pkt_type = p; }
#endif

  enum { USER_ANNO_SIZE = 16,
	 USER_ANNO_US_SIZE = 8,
	 USER_ANNO_S_SIZE = 8,
	 USER_ANNO_U_SIZE = 4,
	 USER_ANNO_I_SIZE = 4 };
  
  uint8_t user_anno_c(int i) const	{ return anno()->user.c[i]; }
  void set_user_anno_c(int i, uint8_t v) { anno()->user.c[i] = v; }
  uint16_t user_anno_us(int i) const	{ return anno()->user.us[i]; }
  void set_user_anno_us(int i, uint16_t v) { anno()->user.us[i] = v; }
  int16_t user_anno_s(int i) const	{ return anno()->user.us[i]; }
  void set_user_anno_s(int i, int16_t v) { anno()->user.s[i] = v; }
  uint32_t user_anno_u(int i) const	{ return anno()->user.u[i]; }
  void set_user_anno_u(int i, uint32_t v) { anno()->user.u[i] = v; }
  int32_t user_anno_i(int i) const	{ return anno()->user.i[i]; }
  void set_user_anno_i(int i, int32_t v) { anno()->user.i[i] = v; }

  const uint8_t *all_user_anno() const	{ return &anno()->user.c[0]; }
  uint8_t *all_user_anno()		{ return &anno()->user.c[0]; }
  const uint32_t *all_user_anno_u() const { return &anno()->user.u[0]; }
  uint32_t *all_user_anno_u()		{ return &anno()->user.u[0]; }
  
  void clear_annotations();
  void copy_annotations(const Packet *);
  
 private:

  // Anno must fit in sk_buff's char cb[48].
  struct Anno {
    union {
      uint8_t c[ADDR_ANNO_SIZE];
      uint32_t ip4;
    } addr;
    
    union {
      uint8_t c[USER_ANNO_SIZE];
      uint16_t us[USER_ANNO_US_SIZE];
      int16_t s[USER_ANNO_S_SIZE];
      uint32_t u[USER_ANNO_U_SIZE];
      int32_t i[USER_ANNO_I_SIZE];
    } user;
    // flag allocations: see packet_anno.hh
    
#if (defined(CLICK_LINUXMODULE) || defined(CLICK_BSDMODULE)) && defined(HAVE_INT64_TYPES)
    uint64_t perfctr;
#endif
  };

#ifndef CLICK_LINUXMODULE
  /*
   * User-space and BSD kernel module implementations.
   */
  int _use_count;
  Packet *_data_packet;
  /* mimic Linux sk_buff */
  unsigned char *_head; /* start of allocated buffer */
  unsigned char *_data; /* where the packet starts */
  unsigned char *_tail; /* one beyond end of packet */
  unsigned char *_end;  /* one beyond end of allocated buffer */
#ifdef CLICK_USERLEVEL
  void (*_destructor)(unsigned char *, size_t);
#endif
  unsigned char _cb[48];
  union {
    unsigned char *raw;
    click_ether *ethernet;
  } _mac;
  union {
    unsigned char *raw;
    click_ip *iph;
    click_ip6 *ip6h;
  } _nh;
  union {
    unsigned char *raw;
    click_tcp *th;
    click_udp *uh;
  } _h;
  PacketType _pkt_type;
  struct timeval _timestamp;
#ifdef CLICK_BSDMODULE
  struct mbuf *_m;
#endif
  Packet *_next;
#endif
  
  Packet();
  Packet(const Packet &);
  ~Packet();
  Packet &operator=(const Packet &);

#ifndef CLICK_LINUXMODULE
  Packet(int, int, int)			{ }
  static WritablePacket *make(int, int, int);
  bool alloc_data(uint32_t, uint32_t, uint32_t);
#endif
#ifdef CLICK_BSDMODULE
  static void assimilate_mbuf(Packet *p);
  void assimilate_mbuf();
#endif

  inline void shift_header_annotations(int32_t shift);
  WritablePacket *expensive_uniqueify(int32_t extra_headroom, int32_t extra_tailroom, bool free_on_failure);
  WritablePacket *expensive_push(uint32_t nbytes);
  WritablePacket *expensive_put(uint32_t nbytes);
  
  friend class WritablePacket;

};


class WritablePacket : public Packet { public:
  
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  unsigned char *data() const			{ return skb()->data; }
  unsigned char *buffer_data() const		{ return skb()->head; }
  unsigned char *mac_header() const		{ return skb()->mac.raw; }
  click_ether *ether_header() const {return (click_ether*)skb()->mac.ethernet;}
  unsigned char *network_header() const		{ return skb()->nh.raw; }
  click_ip *ip_header() const		{ return (click_ip *)skb()->nh.iph; }
  click_ip6 *ip6_header() const         { return (click_ip6*)skb()->nh.ipv6h; }
  unsigned char *transport_header() const	{ return skb()->h.raw; }
  click_tcp *tcp_header() const		{ return (click_tcp*)skb()->h.th; }
  click_udp *udp_header() const		{ return (click_udp*)skb()->h.uh; }
#else				/* User-space or BSD kernel module */
  unsigned char *data() const			{ return _data; }
  unsigned char *buffer_data() const		{ return _head; }
  unsigned char *mac_header() const		{ return _mac.raw; }
  click_ether *ether_header() const		{ return _mac.ethernet; }
  unsigned char *network_header() const		{ return _nh.raw; }
  click_ip *ip_header() const			{ return _nh.iph; }
  click_ip6 *ip6_header() const                 { return _nh.ip6h; }
  unsigned char *transport_header() const	{ return _h.raw; }
  click_tcp *tcp_header() const			{ return _h.th; }
  click_udp *udp_header() const			{ return _h.uh; }
#endif

 private:

  WritablePacket()				{ }
  WritablePacket(const Packet &)		{ }
  ~WritablePacket()				{ }

  friend class Packet;
  
};


inline WritablePacket *
Packet::make(uint32_t len)
{
  return make(DEFAULT_HEADROOM, (const unsigned char *)0, len, 0);
}

inline WritablePacket *
Packet::make(const char *s, uint32_t len)
{
  return make(DEFAULT_HEADROOM, (const unsigned char *)s, len, 0);
}

inline WritablePacket *
Packet::make(const unsigned char *s, uint32_t len)
{
  return make(DEFAULT_HEADROOM, (const unsigned char *)s, len, 0);
}

#ifdef CLICK_LINUXMODULE
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
  skbmgr_recycle_skbs(b);
}
#endif

#ifdef CLICK_BSDMODULE		/* BSD kernel module */
inline void
Packet::assimilate_mbuf(Packet *p)
{
  struct mbuf *m = p->m();

  if (!m) return;
  if (m->m_pkthdr.len != m->m_len)
    click_chatter("assimilate_mbuf(): inconsistent lengths, %d vs. %d", m->m_pkthdr.len, m->m_len);

  p->_head = (unsigned char *)
	     (m->m_flags & M_EXT    ? m->m_ext.ext_buf :
	      m->m_flags & M_PKTHDR ? m->m_pktdat :
				      m->m_dat);
  p->_data = (unsigned char *)m->m_data;
  p->_tail = (unsigned char *)(m->m_data + m->m_len);
  p->_end = p->_head + (
		m->m_flags & M_EXT    ? MCLBYTES :
		m->m_flags & M_PKTHDR ? MHLEN :
					MLEN);
}

inline void
Packet::assimilate_mbuf()
{
  assimilate_mbuf(this);
}

inline Packet *
Packet::make(struct mbuf *m)
{
  if (!(m->m_flags & M_PKTHDR))
    panic("trying to construct Packet from a non-packet mbuf");
  if (m->m_next)
    click_chatter("Yow, constructing Packet from an mbuf chain!");

  Packet *p = new Packet;
  if (m->m_pkthdr.len != m->m_len) {
    /* click needs contiguous data */
    click_chatter("m_pulldown, Click needs contiguous data");
    m = m_pulldown(m, 0, m->m_pkthdr.len, NULL);
  }
  p->_m = m;
  assimilate_mbuf(p);

  return p;
}
#endif

inline bool
Packet::shared() const
{
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  return skb_cloned(const_cast<struct sk_buff *>(skb()));
#else				/* User-space or BSD kernel module */
  return (_data_packet || _use_count > 1);
#endif
}

inline WritablePacket *
Packet::uniqueify()
{
  if (!shared())
    return static_cast<WritablePacket *>(this);
  else
    return expensive_uniqueify(0, 0, true);
}

inline WritablePacket *
Packet::push(uint32_t nbytes)
{
  if (headroom() >= nbytes && !shared()) {
    WritablePacket *q = (WritablePacket *)this;
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_push(q->skb(), nbytes);
#else				/* User-space and BSD kernel module */
    q->_data -= nbytes;
# ifdef CLICK_BSDMODULE
    q->m()->m_data -= nbytes;
    q->m()->m_len += nbytes;
    q->m()->m_pkthdr.len += nbytes;
# endif
#endif
    return q;
  } else
    return expensive_push(nbytes);
}

inline Packet *
Packet::nonunique_push(uint32_t nbytes)
{
  if (headroom() >= nbytes) {
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_push(skb(), nbytes);
#else				/* User-space and BSD kernel module */
    _data -= nbytes;
# ifdef CLICK_BSDMODULE
    m()->m_data -= nbytes;
    m()->m_len += nbytes;
    m()->m_pkthdr.len += nbytes;
# endif
#endif
    return this;
  } else
    return expensive_push(nbytes);
}

/* Get rid of some bytes at the start of a packet */
inline void
Packet::pull(uint32_t nbytes)
{
  if (nbytes > length()) {
    click_chatter("Packet::pull %d > length %d\n", nbytes, length());
    nbytes = length();
  }
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  __skb_pull(skb(), nbytes);
#else				/* User-space and BSD kernel module */
  _data += nbytes;
# ifdef CLICK_BSDMODULE
  m()->m_data += nbytes;
  m()->m_len -= nbytes;
  m()->m_pkthdr.len -= nbytes;
# endif
#endif
}

inline WritablePacket *
Packet::put(uint32_t nbytes)
{
  if (tailroom() >= nbytes && !shared()) {
    WritablePacket *q = (WritablePacket *)this;
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_put(q->skb(), nbytes);
#else				/* User-space and BSD kernel module */
    q->_tail += nbytes;
# ifdef CLICK_BSDMODULE
    q->m()->m_len += nbytes;
    q->m()->m_pkthdr.len += nbytes;
# endif
#endif
    return q;
  } else
    return expensive_put(nbytes);
}

inline Packet *
Packet::nonunique_put(uint32_t nbytes)
{
  if (tailroom() >= nbytes) {
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_put(skb(), nbytes);
#else				/* User-space and BSD kernel module */
    _tail += nbytes;
# ifdef CLICK_BSDMODULE
    m()->m_len += nbytes;
    m()->m_pkthdr.len += nbytes;
# endif
#endif
    return this;
  } else
    return expensive_put(nbytes);
}

/* Get rid of some bytes at the end of a packet */
inline void
Packet::take(uint32_t nbytes)
{
  if (nbytes > length()) {
    click_chatter("Packet::take %d > length %d\n", nbytes, length());
    nbytes = length();
  }
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  skb()->tail -= nbytes;
  skb()->len -= nbytes;
#else				/* User-space and BSD kernel module */
  _tail -= nbytes;
# ifdef CLICK_BSDMODULE
  m()->m_len -= nbytes;
  m()->m_pkthdr.len -= nbytes;
# endif
#endif    
}

#ifdef CLICK_USERLEVEL
inline void
Packet::shrink_data(const unsigned char *d, uint32_t length)
{
  assert(_data_packet);
  if (d > _head && d + length < _end) {
    _head = _data = const_cast<unsigned char *>(d);
    _tail = _end = const_cast<unsigned char *>(d + length);
  }
}
#endif

inline const IP6Address &
Packet::dst_ip6_anno() const
{
  return reinterpret_cast<const IP6Address &>(anno()->addr.c);
}

inline void
Packet::set_dst_ip6_anno(const IP6Address &a)
{
  memcpy(anno()->addr.c, &a, 16);
}

inline IPAddress 
Packet::dst_ip_anno() const
{
  return IPAddress(anno()->addr.ip4);
}

inline void 
Packet::set_dst_ip_anno(IPAddress a)
{ 
  anno()->addr.ip4 = a.addr(); 
}

inline void
Packet::set_mac_header(const unsigned char *h)
{
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  skb()->mac.raw = const_cast<unsigned char *>(h);
#else				/* User-space and BSD kernel module */
  _mac.raw = const_cast<unsigned char *>(h);
#endif
}

inline void
Packet::set_mac_header(const unsigned char *h, uint32_t len)
{
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  skb()->mac.raw = const_cast<unsigned char *>(h);
  skb()->nh.raw = const_cast<unsigned char *>(h) + len;
#else				/* User-space and BSD kernel module */
  _mac.raw = const_cast<unsigned char *>(h);
  _nh.raw = const_cast<unsigned char *>(h) + len;
#endif
}

inline void
Packet::set_ether_header(const click_ether *h)
{
  set_mac_header(reinterpret_cast<const unsigned char *>(h), 14);
}

inline WritablePacket *
Packet::push_mac_header(uint32_t nbytes)
{
  WritablePacket *q;
  if (headroom() >= nbytes && !shared()) {
    q = (WritablePacket *)this;
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_push(q->skb(), nbytes);
#else				/* User-space and BSD kernel module */
    q->_data -= nbytes;
# ifdef CLICK_BSDMODULE
    q->m()->m_data -= nbytes;
    q->m()->m_len += nbytes;
    q->m()->m_pkthdr.len += nbytes;
# endif
#endif
  } else if ((q = expensive_push(nbytes)))
    /* nada */;
  else
    return 0;
  q->set_mac_header(q->data(), nbytes);
  return q;
}

inline void
Packet::set_network_header(const unsigned char *h, uint32_t len)
{
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  skb()->nh.raw = const_cast<unsigned char *>(h);
  skb()->h.raw = const_cast<unsigned char *>(h) + len;
#else				/* User-space and BSD kernel module */
  _nh.raw = const_cast<unsigned char *>(h);
  _h.raw = const_cast<unsigned char *>(h) + len;
#endif
}

inline void
Packet::set_network_header_length(uint32_t len)
{
#ifdef CLICK_LINUXMODULE	/* Linux kernel module */
  skb()->h.raw = skb()->nh.raw + len;
#else				/* User-space and BSD kernel module */
  _h.raw = _nh.raw + len;
#endif
}

inline void
Packet::set_ip_header(const click_ip *iph, uint32_t len)
{
  set_network_header(reinterpret_cast<const unsigned char *>(iph), len);
}

inline void
Packet::set_ip6_header(const click_ip6 *ip6h, uint32_t len)
{
  set_network_header(reinterpret_cast<const unsigned char *>(ip6h), len);
}

inline void
Packet::set_ip6_header(const click_ip6 *ip6h)
{
  set_ip6_header(ip6h, 40);
}

inline int
Packet::mac_header_offset() const
{
  return mac_header() - data();
}

inline uint32_t
Packet::mac_header_length() const
{
  return network_header() - mac_header();
}

inline int
Packet::network_header_offset() const
{
  return network_header() - data();
}

inline uint32_t
Packet::network_header_length() const
{
  return transport_header() - network_header();
}

inline int
Packet::ip_header_offset() const
{
  return network_header_offset();
}

inline uint32_t
Packet::ip_header_length() const
{
  return network_header_length();
}

inline int
Packet::ip6_header_offset() const
{
  return network_header_offset();
}

inline uint32_t
Packet::ip6_header_length() const
{
  return network_header_length();
}

inline int
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
  set_mac_header(0);
  set_network_header(0, 0);
  set_next(0);
}

inline void
Packet::copy_annotations(const Packet *p)
{
  *anno() = *p->anno();
  set_packet_type_anno(p->packet_type_anno());
  set_device_anno(p->device_anno());
  set_timestamp_anno(p->timestamp_anno());
}

inline void
Packet::shift_header_annotations(int32_t shift)
{
#if CLICK_USERLEVEL || CLICK_BSDMODULE
  _mac.raw += (_mac.raw ? shift : 0);
  _nh.raw += (_nh.raw ? shift : 0);
  _h.raw += (_h.raw ? shift : 0);
#else
  struct sk_buff *mskb = skb();
  mskb->mac.raw += (mskb->mac.raw ? shift : 0);
  mskb->nh.raw += (mskb->nh.raw ? shift : 0);
  mskb->h.raw += (mskb->h.raw ? shift : 0);
#endif
}

CLICK_ENDDECLS
#endif
