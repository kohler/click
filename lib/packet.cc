/*
 * packet.{cc,hh} -- a packet structure. In the Linux kernel, a synonym for
 * `struct sk_buff'
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "packet.hh"
#include "glue.hh"
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>

#ifdef __KERNEL__

Packet::Packet()
{
  StaticAssert(sizeof(Anno) <= 48);
  panic("Packet constructor");
}

Packet::~Packet()
{
  panic("Packet destructor");
}

WritablePacket *
Packet::make(unsigned headroom, const unsigned char *data, unsigned len,
	     unsigned tailroom)
{
  unsigned size = len + headroom + tailroom;
  struct sk_buff *skb = alloc_skb(size, GFP_ATOMIC);
  if (skb) {
    skb_reserve(skb, headroom);	// leave some headroom
    skb_put(skb, len);		// leave space for data
    if (data) memcpy(skb->data, data, len);
  } else {
    click_chatter("oops, kernel could not allocate memory for skbuff");
    return 0;
  }
  Packet *p = (Packet *) skb;
  memset(p->anno(), 0, sizeof(Anno));  
  return (WritablePacket *)p;
}

#else /* !__KERNEL__ */

inline
Packet::Packet()
{
  _use_count = 1;
  _data_packet = 0;
  _head = _data = _tail = _end = 0;
  _nh_iph = 0;
  _h_raw = 0;
  memset(_cb, 0, sizeof(_cb));
}

Packet::~Packet()
{
  if (_data_packet) {
    _data_packet->kill();
  } else if (_head) {
    delete[] _head;
    _head = _data = 0;
  }
}

inline WritablePacket *
Packet::make(int, int, int)
{
  return (WritablePacket *)(new Packet(6, 6, 6));
}

void
Packet::alloc_data(unsigned headroom, unsigned len, unsigned tailroom)
{
  unsigned n = len + headroom + tailroom;
  _head = new unsigned char[n];
  _data = _head + headroom;
  _tail = _data + len;
  _end = _head + n;
}

WritablePacket *
Packet::make(unsigned headroom, const unsigned char *data, unsigned len,
	     unsigned tailroom)
{
  WritablePacket *p = new WritablePacket;
  if (p) {
    p->alloc_data(headroom, len, tailroom);
    if (data && p->data()) memcpy(p->data(), data, len);
  }
  return p;
}

#endif /* __KERNEL__ */

//
// UNIQUEIFICATION
//

#ifdef __KERNEL__

Packet *
Packet::clone()
{
  struct sk_buff *n = skb_clone(skb(), GFP_ATOMIC);
  return (Packet *)n;
}

WritablePacket *
Packet::uniqueify_copy()
{
  struct sk_buff *n = skb_copy(skb(), GFP_ATOMIC);
  // all annotations, including IP header annotation, are copied,
  // but IP header will point to garbage if old header was 0
  if (!ip_header()) n->nh.iph = 0;
  if (!ip6_header()) n->nh.ipv6h = 0;
  if (!transport_header()) n->h.raw = 0;
  kill();
  return (WritablePacket *)n;
}

#else /* user level */

Packet *
Packet::clone()
{
  Packet *p = Packet::make(6, 6, 6); // dummy arguments: no initialization
  if (!p) return 0;
  p->_use_count = 1;
  p->_data_packet = (Packet *)this;
  p->_head = _head;
  p->_data = _data;
  p->_tail = _tail;
  p->_end = _end;
  p->copy_annotations(this);
  p->_nh_iph = _nh_iph;
  p->_nh_ip6h = _nh_ip6h;
  p->_h_raw = _h_raw;
  // increment our reference count because of _data_packet reference
  _use_count++;
  return p;
}

WritablePacket *
Packet::uniqueify_copy()
{
  WritablePacket *p = Packet::make(6, 6, 6); // dummy arguments: no initialization
  if (!p) return 0;
  p->_use_count = 1;
  p->_data_packet = 0;
  p->alloc_data(headroom(), length(), tailroom());
  memcpy(p->_data, _data, _tail - _data);
  p->copy_annotations(this);
  if (_nh_iph) {
    p->_nh_iph = (click_ip *)(p->_data + ip_header_offset());
    p->_h_raw = (unsigned char *)(p->_data + transport_header_offset());
    p->_nh_iph = 0;
  } else if (_nh_ip6h) {
    p->_nh_ip6h = (click_ip6 *)(p->_data);
    p->_h_raw = (unsigned char *)(p->_data + transport_header_offset());
    p->_nh_iph = 0;
  } else {
    p->_nh_iph = 0;
    p->_nh_ip6h = 0;
    p->_h_raw = 0;
  }
  memcpy(p->_cb, _cb, sizeof(_cb));
  kill();
  return p;
}

#endif


//
// PUSH AND PULL
//

/*
 * Prepend some empty space before a packet.
 * May kill this packet and return a new one.
 */
WritablePacket *
Packet::expensive_push(unsigned int nbytes)
{
  click_chatter("expensive Packet::push");
#ifdef __KERNEL__
  struct sk_buff *new_skb = skb_realloc_headroom(skb(), nbytes);
  WritablePacket *q =
    reinterpret_cast<WritablePacket *>(Packet::make(new_skb));
  // oops! -- patch from Richard Mortier
  (void)skb_push(q->skb(), nbytes);
#else
  WritablePacket *q = Packet::make(length() + nbytes);
  memcpy(q->data() + nbytes, data(), length());
  memcpy(q->anno(), anno(), sizeof(Anno));
#endif
  kill();
  return q;
}

WritablePacket *
Packet::put(unsigned int nbytes)
{
  if (tailroom() >= nbytes) {
    WritablePacket *p = uniqueify();
#ifdef __KERNEL__
    skb_put(p->skb(), nbytes);
#else
    p->_tail += nbytes;
#endif
    return p;
  } else {
    click_chatter("expensive Packet::put");
#ifdef __KERNEL__
    WritablePacket *q = 0;
    panic("Packet::put");
#else
    WritablePacket *q = Packet::make(headroom(), data(), length(), tailroom() + nbytes);
    if (_nh_iph) {
      q->_nh_iph = (click_ip *)(q->_data + ip_header_offset());
      q->_h_raw = (unsigned char *)(q->_data + transport_header_offset());
    }
    q->copy_annotations(this);
    q->_tail += nbytes;
#endif
    kill();
    return q;
  }
}

void
Packet::take(unsigned int nbytes)
{
  if (nbytes <= length()) {
#ifdef __KERNEL__
    skb()->tail -= nbytes;
    skb()->len -= nbytes;
#else
    _tail -= nbytes;
#endif    
  } else
    click_chatter("Packet::take oops");
}
