#ifndef PACKET_HH
#define PACKET_HH
#include "ipaddress.hh"
#include "glue.hh"
struct click_ip;

class Packet {

  // Anno must fit in sk_buff's char cb[48].
  struct Anno {
    IPAddress dst_ip;
    unsigned char ip_tos;
    unsigned char ip_ttl;
    unsigned short ip_off;
    char mac_broadcast; // flag: MAC address was a broadcast or multicast.
    char fix_ip_src;    // flag: asks FixIPSrc to set ip_src.
    char param_off;     // for ICMP Parameter Problem, byte offset of error.
    char color;         // one of 255 colors set by Paint element.
    int siblings;       // written by IPFlexMonitor, read by Block
#ifdef __KERNEL__
    union {
      cycles_t cycles[4];
      struct {
	unsigned m0[2];
	unsigned m1[2];
      } perf;
    } p;
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
  click_ip *_nh_iph;
  unsigned char *_h_raw;
#endif
  
  Packet();
  Packet(const Packet &);
  ~Packet();
  Packet &operator=(const Packet &);

#ifndef __KERNEL__
  Packet(int, int, int)			{ }
  static Packet *make(int, int, int);
  void alloc_data(unsigned, unsigned, unsigned);
#endif

  Packet *uniqueify_copy();

  Packet *expensive_push(unsigned int nbytes);
  
#ifndef __KERNEL__
  const Anno *anno() const		{ return (const Anno *)_cb; }
  Anno *anno()				{ return (Anno *)_cb; }
#endif
  
  friend class ShutUpCompiler;
  
 public:
  static unsigned default_headroom()	{ return 28; }
  static unsigned default_tailroom(unsigned len) { return (len<56?64-len:8); }
  
  static Packet *make(unsigned);
  static Packet *make(const char *, unsigned);
  static Packet *make(const unsigned char *, unsigned);
  static Packet *make(unsigned, const unsigned char *, unsigned, unsigned);
  
#ifdef __KERNEL__
  /*
   * Wraps a Packet around an existing sk_buff.
   * Packet now owns the sk_buff (ie we don't increment skb->users).
   */
  static Packet *make(struct sk_buff *);
  struct sk_buff *skb() const		{ return (struct sk_buff *)this; }
  struct sk_buff *steal_skb()		{ return skb(); }

 private:
  const Anno *anno() const		{ return (const Anno *)skb()->cb; }
  Anno *anno()				{ return (Anno *)skb()->cb; }
#endif

 public:
#ifdef __KERNEL__
  void kill() 				{ __kfree_skb(skb()); }
#else
  void kill()				{ if (--_use_count <= 0) delete this; }
#endif

  bool shared() const;
  Packet *clone();
  Packet *uniqueify();
  
#ifdef __KERNEL__
  unsigned char *data() const	{ return skb()->data; }
  unsigned length() const	{ return skb()->len; }
  unsigned headroom() const	{ return skb()->data - skb()->head; }
  unsigned tailroom() const	{ return skb()->end - skb()->tail; }
#else
  unsigned char *data() const	{ return _data; }
  unsigned length() const	{ return _tail - _data; }
  unsigned headroom() const	{ return _data - _head; }
  unsigned tailroom() const	{ return _end - _tail; }
#endif
  
  Packet *push(unsigned nbytes);	// Add more space before packet.
  Packet *nonunique_push(unsigned);
  void pull(unsigned nbytes);		// Get rid of initial bytes.
  Packet *put(unsigned nbytes);		// Add bytes to end of pkt.
  Packet *take(unsigned nbytes);	// Delete bytes from end of pkt.

#ifdef __KERNEL__
  click_ip *ip_header() const		{ return (click_ip *)skb()->nh.iph; }
  unsigned char *transport_header() const	{ return skb()->h.raw; }
#else
  click_ip *ip_header() const		{ return _nh_iph; }
  unsigned char *transport_header() const	{ return _h_raw; }
#endif
  void set_ip_header(click_ip *, unsigned);
  unsigned ip_header_offset() const;
  unsigned ip_header_length() const;
  unsigned transport_header_offset() const;

  void copy_annotations(Packet *);
  
  void set_dst_ip_anno(IPAddress a)	{ anno()->dst_ip = a; }
  IPAddress dst_ip_anno() const		{ return anno()->dst_ip; }
  
  void set_ip_tos_anno(unsigned char t)	{ anno()->ip_tos = t; }
  unsigned char ip_tos_anno() const	{ return anno()->ip_tos; }
  void set_ip_ttl_anno(unsigned char t)	{ anno()->ip_ttl = t; }
  unsigned char ip_ttl_anno() const	{ return anno()->ip_ttl; }
  void set_ip_off_anno(unsigned short o){ anno()->ip_off = o; }
  unsigned short ip_off_anno() const    { return anno()->ip_off; }
  void set_mac_broadcast_anno(char b)	{ anno()->mac_broadcast = b; }
  char mac_broadcast_anno() const	{ return anno()->mac_broadcast; }
  void set_fix_ip_src_anno(char f)	{ anno()->fix_ip_src = f; }
  char fix_ip_src_anno() const		{ return anno()->fix_ip_src; }
  void set_param_off_anno(char p)	{ anno()->param_off = p; }
  char param_off_anno() const		{ return anno()->param_off; }
  void set_color_anno(char c)		{ anno()->color = c; }
  char color_anno() const		{ return anno()->color; }
  void set_siblings_anno(int s)	        { anno()->siblings = s; }
  int siblings_anno() const		{ return anno()->siblings; }
#ifdef __KERNEL__
  void set_cycle_anno(int i, cycles_t v) { anno()->p.cycles[i] = v; }
  void set_metric0_anno(int i, unsigned v) { anno()->p.perf.m0[i] = v; }
  void set_metric1_anno(int i, unsigned v) { anno()->p.perf.m1[i] = v; }
  cycles_t cycle_anno(int i) const	{ return anno()->p.cycles[i]; }
  unsigned metric0_anno(int i) const	{ return anno()->p.perf.m0[i]; }
  unsigned metric1_anno(int i) const	{ return anno()->p.perf.m1[i]; }
#endif
};


inline Packet *
Packet::make(unsigned len)
{
  return make(default_headroom(), (const unsigned char *)0, len,
	      default_tailroom(len));
}

inline Packet *
Packet::make(const char *s, unsigned len)
{
  return make(default_headroom(), (const unsigned char *)s, len,
	      default_tailroom(len));
}

inline Packet *
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
    return (Packet *)skb;
  else
    return (Packet *)skb_clone(skb, GFP_ATOMIC);
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

inline Packet *
Packet::uniqueify()
{
  if (shared())
    return uniqueify_copy();
  else
    return this;
}

inline Packet *
Packet::push(unsigned int nbytes)
{
  if (headroom() >= nbytes) {
    Packet *p = uniqueify();
#ifdef __KERNEL__
    skb_push(p->skb(), nbytes);
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
    skb_push(skb(), nbytes);
#else
    _data -= nbytes;
#endif
    return this;
  } else
    return expensive_push(nbytes);
}

/*
 * Get rid of some bytes at the start of a packet.
 */
inline void
Packet::pull(unsigned int nbytes)
{
  if (nbytes > length()) {
    click_chatter("Packet::pull %d > length %d\n", nbytes, length());
    nbytes = length();
  }
#ifdef __KERNEL__
  skb_pull(skb(), nbytes);
#else
  _data += nbytes;
#endif
}

inline void
Packet::set_ip_header(click_ip *iph, unsigned len)
{
#ifdef __KERNEL__
  skb()->nh.iph = (struct iphdr *)iph;
  skb()->h.raw = (unsigned char *)iph + len;
#else
  _nh_iph = iph;
  _h_raw = (unsigned char *)iph + len;
#endif
}

inline unsigned
Packet::ip_header_offset() const
{
  return (unsigned char *)ip_header() - data();
}

inline unsigned
Packet::ip_header_length() const
{
  return (unsigned char *)transport_header() - (unsigned char *)ip_header();
}

inline unsigned
Packet::transport_header_offset() const
{
  return (unsigned char *)transport_header() - data();
}

inline void
Packet::copy_annotations(Packet *p)
{
  *anno() = *p->anno();
}

#endif
