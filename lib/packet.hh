#ifndef PACKET_HH
#define PACKET_HH
#include "ipaddress.hh"
#include "glue.hh"

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
#endif
  
  Packet();
  Packet(const Packet &);
  ~Packet();
  Packet &operator=(const Packet &);

#ifndef __KERNEL__
  Packet(int, int, int)			{ }
  static Packet *make(int, int, int);
  void alloc_data(int len);
#endif

  Packet *uniqueify_copy();
  
#ifdef __KERNEL__
  const Anno *anno() const		{ return (const Anno *)skb()->cb; }
  Anno *anno()				{ return (Anno *)skb()->cb; }
#else
  const Anno *anno() const		{ return (const Anno *)_cb; }
  Anno *anno()				{ return (Anno *)_cb; }
#endif
  
  friend class ShutUpCompiler;
  
 public:
  
  static Packet *make(unsigned);
  static Packet *make(const char *, unsigned);
  static Packet *make(const unsigned char *, unsigned);
  
#ifdef __KERNEL__
  /*
   * Wraps a Packet around an existing sk_buff.
   * Packet now owns the sk_buff (ie we don't increment skb->users).
   */
  static Packet *make(struct sk_buff *);
  struct sk_buff *skb() const		{ return (struct sk_buff *)this; }
  struct sk_buff *steal_skb()		{ return skb(); }
#endif

#ifdef __KERNEL__
  // unuse() already took care of atomic_dec_and_test
  void kill()				{ __kfree_skb(skb()); }
#else
  void kill()				{ if (--_use_count <= 0) delete this; }
#endif
  
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
  
  Packet *push(unsigned int nbytes);	// Add more space before packet.
  void pull(unsigned int nbytes);	// Get rid of initial bytes.
  Packet *put(unsigned int nbytes);	// Add bytes to end of pkt.
  Packet *take(unsigned int nbytes);	// Delete bytes from end of pkt.
  
#ifndef __KERNEL__
  /* for Spew(), not checked for speed */
  void spew_push(int zz) { _data -= zz; }
#endif

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
  
};


inline Packet *
Packet::make(unsigned len)
{
  return make((const unsigned char *)0, len);
}

inline Packet *
Packet::make(const char *s, unsigned len)
{
  return make((const unsigned char *)s, len);
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

inline Packet *
Packet::uniqueify()
{
#ifdef __KERNEL__
  if (skb_cloned(skb()))
#else
  if (_data_packet || _use_count > 1)
#endif
    return uniqueify_copy();
  else
    return this;
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

#endif
