// -*- related-file-name: "../../lib/packet.cc" -*-
#ifndef CLICK_PACKET_HH
#define CLICK_PACKET_HH
#include <click/ipaddress.hh>
#include <click/ip6address.hh>
#include <click/glue.hh>
#include <click/timestamp.hh>
#if CLICK_LINUXMODULE
# include <click/skbmgr.hh>
#endif
struct click_ether;
struct click_ip;
struct click_icmp;
struct click_ip6;
struct click_tcp;
struct click_udp;

#if CLICK_NS
# include <click/simclick.h>
#endif


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
  
#if CLICK_LINUXMODULE
  // Packet::make(sk_buff *) wraps a Packet around an existing sk_buff.
  // Packet now owns the sk_buff (ie we don't increment skb->users).
  static Packet *make(struct sk_buff *);
  struct sk_buff *skb()			{ return (struct sk_buff *)this; }
  const struct sk_buff *skb() const	{ return (const struct sk_buff*)this; }
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

    inline bool shared() const;
    Packet *clone();
    WritablePacket *uniqueify();
  
    inline const unsigned char *data() const;
    inline const unsigned char *end_data() const;
    inline uint32_t length() const;
    inline uint32_t headroom() const;
    inline uint32_t tailroom() const;
    inline const unsigned char *buffer_data() const;
    inline uint32_t buffer_length() const;
  
    WritablePacket *push(uint32_t nb);	// Add more space before packet.
    WritablePacket *push_mac_header(uint32_t nb);
    Packet *nonunique_push(uint32_t nb);
    void pull(uint32_t nb);		// Get rid of initial bytes.
    WritablePacket *put(uint32_t nb);	// Add bytes to end of pkt.
    Packet *nonunique_put(uint32_t nb);
    void take(uint32_t nb);		// Delete bytes from end of pkt.

    Packet *shift_data(int offset, bool free_on_failure = true);
#if CLICK_USERLEVEL
    inline void shrink_data(const unsigned char *, uint32_t length);
    inline void change_headroom_and_length(uint32_t headroom, uint32_t length);
#endif

    // HEADER ANNOTATIONS
    inline const unsigned char *mac_header() const;
    inline void set_mac_header(const unsigned char *);
    inline void set_mac_header(const unsigned char *, uint32_t);
    inline int mac_header_offset() const;
    inline uint32_t mac_header_length() const;
    inline int mac_length() const;

    inline const unsigned char *network_header() const;
    inline void set_network_header(const unsigned char *, uint32_t);
    inline void set_network_header_length(uint32_t);
    inline int network_header_offset() const;
    inline uint32_t network_header_length() const;
    inline int network_length() const;

    inline const unsigned char *transport_header() const;
    inline int transport_header_offset() const;
    inline int transport_length() const;

    // CONVENIENCE HEADER ANNOTATIONS
    inline const click_ether *ether_header() const;
    inline void set_ether_header(const click_ether *);
  
    inline const click_ip *ip_header() const;
    inline void set_ip_header(const click_ip *, uint32_t);
    inline int ip_header_offset() const;
    inline uint32_t ip_header_length() const;

    inline const click_ip6 *ip6_header() const;
    inline void set_ip6_header(const click_ip6 *);
    inline void set_ip6_header(const click_ip6 *, uint32_t);
    inline int ip6_header_offset() const;
    inline uint32_t ip6_header_length() const;

    inline const click_icmp *icmp_header() const;
    inline const click_tcp *tcp_header() const;
    inline const click_udp *udp_header() const;

    // LINKS
    inline Packet *next() const;
    inline Packet *&next();
    inline void set_next(Packet *p);
  
    // ANNOTATIONS

 private:
  struct Anno;
#if CLICK_LINUXMODULE	/* Linux kernel module */
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

    inline const Timestamp &timestamp_anno() const;
    inline Timestamp &timestamp_anno();
    inline void set_timestamp_anno(const Timestamp &);

    inline net_device *device_anno() const;
    inline void set_device_anno(net_device *);

    inline PacketType packet_type_anno() const;
    inline void set_packet_type_anno(PacketType);
    
#if CLICK_LINUXMODULE
# ifdef HAVE_INT64_TYPES
  uint64_t perfctr_anno() const		{ return anno()->perfctr; }
  void set_perfctr_anno(uint64_t pc)	{ anno()->perfctr = pc; }
# endif

#else			/* User-space and BSD kernel module */

#if CLICK_NS
  class SimPacketinfoWrapper {
  public:
    simclick_simpacketinfo _pinfo;
    SimPacketinfoWrapper() {
      // The uninitialized value for the simulator packet data can't be 
      // all zeros (0 is a valid packet id) or random junk out of memory
      // since the simulator will look at this info to see if the packet
      // was originally generated by it. Accidental collisions with other
      // packet IDs or bogus packet IDs can cause weird things to happen. So we
      // set it to all -1 here to keep the simulator from getting confused.
      memset(&_pinfo,-1,sizeof(_pinfo));
    }
  };
  simclick_simpacketinfo*  get_sim_packetinfo() {
    return &(_sim_packetinfo._pinfo);
  }
  void set_sim_packetinfo(simclick_simpacketinfo* pinfo) { 
    _sim_packetinfo._pinfo = *pinfo;
  }
#endif
#endif

  enum { USER_ANNO_SIZE = 24,
	 USER_ANNO_US_SIZE = 12,
	 USER_ANNO_S_SIZE = 12,
	 USER_ANNO_U_SIZE = 6,
	 USER_ANNO_I_SIZE = 6 };
  
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
	    char ch[ADDR_ANNO_SIZE];
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
    
#if (CLICK_LINUXMODULE || CLICK_BSDMODULE) && defined(HAVE_INT64_TYPES)
	uint64_t perfctr;
#endif
    };

#if !CLICK_LINUXMODULE
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
# if CLICK_USERLEVEL
  void (*_destructor)(unsigned char *, size_t);
# endif
    unsigned char _cb[48];
    unsigned char *_mac;
    unsigned char *_nh;
    unsigned char *_h;
    PacketType _pkt_type;
    Timestamp _timestamp;
# if CLICK_BSDMODULE
  struct mbuf *_m;
# endif
  Packet *_next;
# if CLICK_NS
  SimPacketinfoWrapper _sim_packetinfo;
# endif
#endif
  
  Packet();
  Packet(const Packet &);
  ~Packet();
  Packet &operator=(const Packet &);

#if !CLICK_LINUXMODULE
  Packet(int, int, int)			{ }
  static WritablePacket *make(int, int, int);
  bool alloc_data(uint32_t, uint32_t, uint32_t);
#endif
#if CLICK_BSDMODULE
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
  
    inline unsigned char *data() const;
    inline unsigned char *end_data() const;
    inline unsigned char *buffer_data() const;
    inline unsigned char *mac_header() const;
    inline click_ether *ether_header() const;
    inline unsigned char *network_header() const;
    inline click_ip *ip_header() const;
    inline click_ip6 *ip6_header() const;
    inline unsigned char *transport_header() const;
    inline click_icmp *icmp_header() const;
    inline click_tcp *tcp_header() const;
    inline click_udp *udp_header() const;
    
 private:

    WritablePacket()				{ }
    WritablePacket(const Packet &)		{ }
    ~WritablePacket()				{ }

    friend class Packet;
  
};



inline const unsigned char *
Packet::data() const
{
#if CLICK_LINUXMODULE
    return skb()->data;
#else
    return _data;
#endif
}

inline const unsigned char *
Packet::end_data() const
{
#if CLICK_LINUXMODULE
    return skb()->tail;
#else
    return _tail;
#endif
}

inline uint32_t
Packet::length() const
{
#if CLICK_LINUXMODULE
    return skb()->len;
#else
    return _tail - _data;
#endif
}

inline uint32_t
Packet::headroom() const
{
#if CLICK_LINUXMODULE
    return skb()->data - skb()->head;
#else
    return _data - _head;
#endif
}

inline uint32_t
Packet::tailroom() const
{
#if CLICK_LINUXMODULE
    return skb()->end - skb()->tail;
#else
    return _end - _tail;
#endif
}

inline const unsigned char *
Packet::buffer_data() const
{
#if CLICK_LINUXMODULE
    return skb()->head;
#else
    return _head;
#endif
}

inline uint32_t
Packet::buffer_length() const
{
#if CLICK_LINUXMODULE
    return skb()->end - skb()->head;
#else
    return _end - _head;
#endif
}


inline Packet *
Packet::next() const
{
#if CLICK_LINUXMODULE
    return (Packet *)(skb()->next);
#else
    return _next;
#endif
}

inline Packet *&
Packet::next()
{
#if CLICK_LINUXMODULE
    return (Packet *&)(skb()->next);
#else
    return _next;
#endif
}

inline void
Packet::set_next(Packet *p)
{
#if CLICK_LINUXMODULE
    skb()->next = p->skb();
#else
    _next = p;
#endif
}

inline const unsigned char *
Packet::mac_header() const
{
#if CLICK_LINUXMODULE
    return skb()->mac.raw;
#else
    return _mac;
#endif
}

inline const unsigned char *
Packet::network_header() const
{
#if CLICK_LINUXMODULE
    return skb()->nh.raw;
#else
    return _nh;
#endif
}

inline const unsigned char *
Packet::transport_header() const
{
#if CLICK_LINUXMODULE
    return skb()->h.raw;
#else
    return _h;
#endif
}

inline const click_ether *
Packet::ether_header() const
{
    return reinterpret_cast<const click_ether *>(mac_header());
}

inline const click_ip *
Packet::ip_header() const
{
    return reinterpret_cast<const click_ip *>(network_header());
}

inline const click_ip6 *
Packet::ip6_header() const
{
    return reinterpret_cast<const click_ip6 *>(network_header());
}

inline const click_icmp *
Packet::icmp_header() const
{
    return reinterpret_cast<const click_icmp *>(transport_header());
}

inline const click_tcp *
Packet::tcp_header() const
{
    return reinterpret_cast<const click_tcp *>(transport_header());
}

inline const click_udp *
Packet::udp_header() const
{
    return reinterpret_cast<const click_udp *>(transport_header());
}

inline int
Packet::mac_length() const
{
    return end_data() - mac_header();
}

inline int
Packet::network_length() const
{
    return end_data() - network_header();
}

inline int
Packet::transport_length() const
{
    return end_data() - transport_header();
}

inline const Timestamp &
Packet::timestamp_anno() const
{
#if CLICK_LINUXMODULE
# if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 13)
    return *(const Timestamp *) &skb()->stamp;
# else
    return *(const Timestamp *) &skb()->tstamp;
# endif
#else
    return _timestamp;
#endif
}

inline Timestamp &
Packet::timestamp_anno()
{
#if CLICK_LINUXMODULE
# if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 13)
    return *(Timestamp *) &skb()->stamp;
# else
    return *(Timestamp *) &skb()->tstamp;
# endif
#else
    return _timestamp;
#endif
}

inline void
Packet::set_timestamp_anno(const Timestamp &timestamp)
{
#if CLICK_LINUXMODULE
# if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 13)
    memcpy(&skb()->stamp, &timestamp, 8);
# else
    skb_set_timestamp(skb(), &timestamp.timeval());
# endif
#else
    _timestamp = timestamp;
#endif
}

inline net_device *
Packet::device_anno() const
{
#if CLICK_LINUXMODULE
    return skb()->dev;
#elif CLICK_BSDMODULE
    if (m())
	return m()->m_pkthdr.rcvif;
    else
	return 0;
#else
    return 0;
#endif
}

inline void
Packet::set_device_anno(net_device *dev)
{
#if CLICK_LINUXMODULE
    skb()->dev = dev;
#elif CLICK_BSDMODULE
    if (m())
	m()->m_pkthdr.rcvif = dev;
#else
    (void) dev;
#endif
}

inline Packet::PacketType
Packet::packet_type_anno() const
{
#if CLICK_LINUXMODULE
    return (PacketType)(skb()->pkt_type & PACKET_TYPE_MASK);
#else
    return _pkt_type;
#endif
}

inline void
Packet::set_packet_type_anno(PacketType p)
{
#if CLICK_LINUXMODULE
    skb()->pkt_type = (skb()->pkt_type & PACKET_CLEAN) | p;
#else
    _pkt_type = p;
#endif
}

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

#if CLICK_LINUXMODULE
inline Packet *
Packet::make(struct sk_buff *skb)
{
  if (atomic_read(&skb->users) == 1) {
    skb_orphan(skb);
    return reinterpret_cast<Packet *>(skb);
  } else {
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
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 15)
    b->list = 0;
#endif
    skbmgr_recycle_skbs(b);
}
#endif

#if CLICK_BSDMODULE		/* BSD kernel module */
inline void
Packet::assimilate_mbuf(Packet *p)
{
  struct mbuf *m = p->m();

  if (!m) return;

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

  Packet *p = new Packet;
  if (m->m_pkthdr.len != m->m_len) {
    /* click needs contiguous data */
    // click_chatter("m_pulldown, Click needs contiguous data");

    if (m_pulldown(m, 0, m->m_pkthdr.len, NULL) == NULL)
	panic("m_pulldown failed");
  }
  p->_m = m;
  assimilate_mbuf(p);

  return p;
}
#endif

inline bool
Packet::shared() const
{
#if CLICK_LINUXMODULE
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
    return expensive_uniqueify(0, 0, true);
}

inline WritablePacket *
Packet::push(uint32_t nbytes)
{
  if (headroom() >= nbytes && !shared()) {
    WritablePacket *q = (WritablePacket *)this;
#if CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_push(q->skb(), nbytes);
#else				/* User-space and BSD kernel module */
    q->_data -= nbytes;
# if CLICK_BSDMODULE
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
#if CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_push(skb(), nbytes);
#else				/* User-space and BSD kernel module */
    _data -= nbytes;
# if CLICK_BSDMODULE
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
#if CLICK_LINUXMODULE	/* Linux kernel module */
  __skb_pull(skb(), nbytes);
#else				/* User-space and BSD kernel module */
  _data += nbytes;
# if CLICK_BSDMODULE
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
#if CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_put(q->skb(), nbytes);
#else				/* User-space and BSD kernel module */
    q->_tail += nbytes;
# if CLICK_BSDMODULE
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
#if CLICK_LINUXMODULE	/* Linux kernel module */
    __skb_put(skb(), nbytes);
#else				/* User-space and BSD kernel module */
    _tail += nbytes;
# if CLICK_BSDMODULE
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
#if CLICK_LINUXMODULE	/* Linux kernel module */
  skb()->tail -= nbytes;
  skb()->len -= nbytes;
#else				/* User-space and BSD kernel module */
  _tail -= nbytes;
# if CLICK_BSDMODULE
  m()->m_len -= nbytes;
  m()->m_pkthdr.len -= nbytes;
# endif
#endif
}

#if CLICK_USERLEVEL
inline void
Packet::shrink_data(const unsigned char *d, uint32_t length)
{
  assert(_data_packet);
  if (d >= _head && d + length >= d && d + length <= _end) {
    _head = _data = const_cast<unsigned char *>(d);
    _tail = _end = const_cast<unsigned char *>(d + length);
  }
}

inline void
Packet::change_headroom_and_length(uint32_t headroom, uint32_t length)
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
    return *reinterpret_cast<const IP6Address *>(anno()->addr.ch);
}

inline void
Packet::set_dst_ip6_anno(const IP6Address &a)
{
    memcpy(anno()->addr.ch, &a, 16);
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
#if CLICK_LINUXMODULE	/* Linux kernel module */
    skb()->mac.raw = const_cast<unsigned char *>(h);
#else				/* User-space and BSD kernel module */
    _mac = const_cast<unsigned char *>(h);
#endif
}

inline void
Packet::set_mac_header(const unsigned char *h, uint32_t len)
{
#if CLICK_LINUXMODULE	/* Linux kernel module */
    skb()->mac.raw = const_cast<unsigned char *>(h);
    skb()->nh.raw = const_cast<unsigned char *>(h) + len;
#else				/* User-space and BSD kernel module */
    _mac = const_cast<unsigned char *>(h);
    _nh = const_cast<unsigned char *>(h) + len;
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
#if CLICK_LINUXMODULE	/* Linux kernel module */
	__skb_push(q->skb(), nbytes);
#else				/* User-space and BSD kernel module */
	q->_data -= nbytes;
# if CLICK_BSDMODULE
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
#if CLICK_LINUXMODULE	/* Linux kernel module */
    skb()->nh.raw = const_cast<unsigned char *>(h);
    skb()->h.raw = const_cast<unsigned char *>(h) + len;
#else				/* User-space and BSD kernel module */
    _nh = const_cast<unsigned char *>(h);
    _h = const_cast<unsigned char *>(h) + len;
#endif
}

inline void
Packet::set_network_header_length(uint32_t len)
{
#if CLICK_LINUXMODULE	/* Linux kernel module */
    skb()->h.raw = skb()->nh.raw + len;
#else				/* User-space and BSD kernel module */
    _h = _nh + len;
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
  set_timestamp_anno(Timestamp());
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
  _mac += (_mac ? shift : 0);
  _nh += (_nh ? shift : 0);
  _h += (_h ? shift : 0);
#else
  struct sk_buff *mskb = skb();
  mskb->mac.raw += (mskb->mac.raw ? shift : 0);
  mskb->nh.raw += (mskb->nh.raw ? shift : 0);
  mskb->h.raw += (mskb->h.raw ? shift : 0);
#endif
}


inline unsigned char *
WritablePacket::data() const
{
    return const_cast<unsigned char *>(Packet::data());
}

inline unsigned char *
WritablePacket::end_data() const
{
    return const_cast<unsigned char *>(Packet::end_data());
}

inline unsigned char *
WritablePacket::buffer_data() const
{
    return const_cast<unsigned char *>(Packet::buffer_data());
}

inline unsigned char *
WritablePacket::mac_header() const
{
    return const_cast<unsigned char *>(Packet::mac_header());
}

inline unsigned char *
WritablePacket::network_header() const
{
    return const_cast<unsigned char *>(Packet::network_header());
}

inline unsigned char *
WritablePacket::transport_header() const
{
    return const_cast<unsigned char *>(Packet::transport_header());
}

inline click_ether *
WritablePacket::ether_header() const
{
    return const_cast<click_ether *>(Packet::ether_header());
}

inline click_ip *
WritablePacket::ip_header() const
{
    return const_cast<click_ip *>(Packet::ip_header());
}

inline click_ip6 *
WritablePacket::ip6_header() const
{
    return const_cast<click_ip6 *>(Packet::ip6_header());
}

inline click_icmp *
WritablePacket::icmp_header() const
{
    return const_cast<click_icmp *>(Packet::icmp_header());
}

inline click_tcp *
WritablePacket::tcp_header() const
{
    return const_cast<click_tcp *>(Packet::tcp_header());
}

inline click_udp *
WritablePacket::udp_header() const
{
    return const_cast<click_udp *>(Packet::udp_header());
}

CLICK_ENDDECLS
#endif
