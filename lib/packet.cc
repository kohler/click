// -*- c-basic-offset: 2; related-file-name: "../include/click/packet.hh" -*-
/*
 * packet.{cc,hh} -- a packet structure. In the Linux kernel, a synonym for
 * `struct sk_buff'
 * Eddie Kohler, Robert Morris, Nickolai Zeldovich
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/packet.hh>
#include <click/glue.hh>
#ifdef CLICK_USERLEVEL
#include <unistd.h>
#endif
CLICK_DECLS

#ifdef CLICK_LINUXMODULE	/* Linux kernel module */

Packet::Packet()
{
  static_assert(sizeof(Anno) <= sizeof(((struct sk_buff *)0)->cb));
  panic("Packet constructor");
}

Packet::~Packet()
{
  panic("Packet destructor");
}

WritablePacket *
Packet::make(uint32_t headroom, const unsigned char *data,
	     uint32_t len, uint32_t tailroom)
{
  int want = 1;
  if (struct sk_buff *skb = skbmgr_allocate_skbs(headroom, len + tailroom, &want)) {
    assert(want == 1);
    // packet comes back from skbmgr with headroom reserved
    __skb_put(skb, len);	// leave space for data
    if (data) memcpy(skb->data, data, len);
    skb->pkt_type = HOST | PACKET_CLEAN;
    WritablePacket *q = reinterpret_cast<WritablePacket *>(skb);
    q->clear_annotations();
    return q;
  } else
    return 0;
}

#else		/* User-space or BSD kernel module */

inline
Packet::Packet()
{
  _use_count = 1;
  _data_packet = 0;
  _head = _data = _tail = _end = 0;
#if CLICK_USERLEVEL
  _destructor = 0;
#elif CLICK_BSDMODULE
  _m = 0;
#endif
  clear_annotations();
}

Packet::~Packet()
{
  if (_data_packet)
    _data_packet->kill();
#if CLICK_USERLEVEL
  else if (_head && _destructor)
    _destructor(_head, _end - _head);
  else
    delete[] _head;
#elif CLICK_BSDMODULE
  else
    m_freem(_m);
#endif
  _head = _data = 0;
}

inline WritablePacket *
Packet::make(int, int, int)
{
  return static_cast<WritablePacket *>(new Packet(6, 6, 6));
}

#ifdef CLICK_USERLEVEL

WritablePacket *
Packet::make(unsigned char *data, uint32_t len, void (*destruct)(unsigned char *, size_t))
{
  WritablePacket *p = new WritablePacket;
  if (p) {
    p->_head = p->_data = data;
    p->_tail = p->_end = data + len;
    p->_destructor = destruct;
  }
  return p;
}

#endif

bool
Packet::alloc_data(uint32_t headroom, uint32_t len, uint32_t tailroom)
{
  uint32_t n = len + headroom + tailroom;
  if (n < MIN_BUFFER_LENGTH) {
    tailroom = MIN_BUFFER_LENGTH - len - headroom;
    n = MIN_BUFFER_LENGTH;
  }
#if CLICK_USERLEVEL
  unsigned char *d = new unsigned char[n];
  if (!d)
    return false;
  _head = d;
  _data = d + headroom;
  _tail = _data + len;
  _end = _head + n;
#elif CLICK_BSDMODULE
  if (n > MCLBYTES) {
    click_chatter("trying to allocate %d bytes: too many\n", n);
    return false;
  }
  struct mbuf *m;
  MGETHDR(m, M_WAIT, MT_DATA);
  if (!m)
    return false;
  if (n > MHLEN) {
    MCLGET(m, M_WAIT);
    if (!(m->m_flags & M_EXT)) {
      m_freem(m);
      return false;
    }
  }
  _m = m;
  _m->m_data += headroom;
  _m->m_len = len;
  _m->m_pkthdr.len = len;
  assimilate_mbuf();
#endif
  return true;
}

WritablePacket *
Packet::make(uint32_t headroom, const unsigned char *data, uint32_t len,
	     uint32_t tailroom)
{
  WritablePacket *p = new WritablePacket;
  if (!p)
    return 0;
  if (!p->alloc_data(headroom, len, tailroom)) {
    delete p;
    return 0;
  }
  if (data)
    memcpy(p->data(), data, len);
  return p;
}

#endif /* CLICK_LINUXMODULE */


//
// UNIQUEIFICATION
//

Packet *
Packet::clone()
{
#if CLICK_LINUXMODULE
  
  struct sk_buff *nskb = skb_clone(skb(), GFP_ATOMIC);
  return reinterpret_cast<Packet *>(nskb);
  
#elif CLICK_USERLEVEL || CLICK_BSDMODULE
  
  // timing: .31-.39 normal, .43-.55 two allocs, .55-.58 two memcpys
  Packet *p = Packet::make(6, 6, 6); // dummy arguments: no initialization
  if (!p)
    return 0;
  memcpy(p, this, sizeof(Packet));
  p->_use_count = 1;
  p->_data_packet = this;
# if CLICK_USERLEVEL
  p->_destructor = 0;
# else
  p->_m = 0;
# endif
  // increment our reference count because of _data_packet reference
  _use_count++;
  return p;
  
#endif /* CLICK_LINUXMODULE */
}

WritablePacket *
Packet::expensive_uniqueify(int32_t extra_headroom, int32_t extra_tailroom,
			    bool free_on_failure)
{
  assert(extra_headroom >= (int32_t)(-headroom()) && extra_tailroom >= (int32_t)(-tailroom()));
  
#if CLICK_LINUXMODULE
  
  struct sk_buff *nskb = skb();
  unsigned char *old_head = nskb->head;
  uint32_t old_headroom = headroom(), old_length = length();
  
  uint32_t size = buffer_length() + extra_headroom + extra_tailroom;
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
  size = ((size + 15) & ~15); 
  unsigned char *new_data = reinterpret_cast<unsigned char *>(kmalloc(size + sizeof(atomic_t), GFP_ATOMIC));
# else
  size = SKB_DATA_ALIGN(size);
  unsigned char *new_data = reinterpret_cast<unsigned char *>(kmalloc(size + sizeof(struct skb_shared_info), GFP_ATOMIC));
# endif
  if (!new_data) {
    if (free_on_failure)
      kill();
    return 0;
  }

  unsigned char *start_copy = old_head + (extra_headroom >= 0 ? 0 : -extra_headroom);
  unsigned char *end_copy = old_head + buffer_length() + (extra_tailroom >= 0 ? 0 : extra_tailroom);
  memcpy(new_data + (extra_headroom >= 0 ? extra_headroom : 0), start_copy, end_copy - start_copy);

# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
  if (!nskb->cloned || atomic_dec_and_test(skb_datarefp(nskb)))
    kfree(old_head);
# else
  if (!nskb->cloned || atomic_dec_and_test(&(skb_shinfo(nskb)->dataref))) {
    assert(!skb_shinfo(nskb)->nr_frags && !skb_shinfo(nskb)->frag_list);
    kfree(old_head);
  }
# endif
  
  nskb->head = new_data;
  nskb->data = new_data + old_headroom + extra_headroom;
  nskb->tail = nskb->data + old_length;
  nskb->end = new_data + size;
  nskb->len = old_length;
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
  nskb->is_clone = 0;
# endif
  nskb->cloned = 0;

# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
  nskb->truesize = size;
  atomic_set(skb_datarefp(nskb), 1);
# else
  nskb->truesize = size + sizeof(struct sk_buff);
  atomic_set(&(skb_shinfo(nskb)->dataref), 1);
  skb_shinfo(nskb)->nr_frags = 0;
  skb_shinfo(nskb)->frag_list = 0;
# endif

  shift_header_annotations(nskb->head + extra_headroom - old_head);
  return static_cast<WritablePacket *>(this);

#else		/* User-level or BSD kernel module */

  // If someone else has cloned this packet, then we need to leave its data
  // pointers around. Make a clone and uniqueify that.
  if (_use_count > 1) {
    Packet *p = clone();
    WritablePacket *q = (p ? p->expensive_uniqueify(extra_headroom, extra_tailroom, true) : 0);
    if (q || free_on_failure)
      kill();
    return q;
  }
  
  uint8_t *old_head = _head, *old_end = _end;
# if CLICK_BSDMODULE
  struct mbuf *old_m = _m;
# endif
  
  if (!alloc_data(headroom() + extra_headroom, length(), tailroom() + extra_tailroom)) {
    if (free_on_failure)
      kill();
    return 0;
  }
  
  unsigned char *start_copy = old_head + (extra_headroom >= 0 ? 0 : -extra_headroom);
  unsigned char *end_copy = old_end + (extra_tailroom >= 0 ? 0 : extra_tailroom);
  memcpy(_head + (extra_headroom >= 0 ? extra_headroom : 0), start_copy, end_copy - start_copy);

  // free old data
  if (_data_packet)
    _data_packet->kill();
# if CLICK_USERLEVEL
  else if (_destructor)
    _destructor(old_head, old_end - old_head);
  else
    delete[] old_head;
  _destructor = 0;
# elif CLICK_BSDMODULE
  else
    m_freem(old_m);
# endif

  _use_count = 1;
  _data_packet = 0;
  shift_header_annotations(_head + extra_headroom - old_head);
  return static_cast<WritablePacket *>(this);
  
#endif /* CLICK_LINUXMODULE */
}



#ifdef CLICK_BSDMODULE		/* BSD kernel module */

struct mbuf *
Packet::steal_m()
{
  struct Packet *p;
  struct mbuf *m2;

  p = uniqueify();
  m2 = p->m();

  /* Clear the mbuf from the packet: otherwise kill will MFREE it */
  p->_m = 0;
  p->kill();
  return m2;
}

#endif /* CLICK_BSDMODULE */

//
// EXPENSIVE_PUSH, EXPENSIVE_PUT
//

/*
 * Prepend some empty space before a packet.
 * May kill this packet and return a new one.
 */
WritablePacket *
Packet::expensive_push(uint32_t nbytes)
{
  static int chatter = 0;
  if (headroom() < nbytes && chatter < 5) {
    click_chatter("expensive Packet::push; have %d wanted %d",
                  headroom(), nbytes);
    chatter++;
  }
  if (WritablePacket *q = expensive_uniqueify((nbytes + 128) & ~3, 0, true)) {
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
    return 0;
}

WritablePacket *
Packet::expensive_put(uint32_t nbytes)
{
  static int chatter = 0;
  if (tailroom() < nbytes && chatter < 5) {
    click_chatter("expensive Packet::put; have %d wanted %d",
                  tailroom(), nbytes);
    chatter++;
  }
  if (WritablePacket *q = expensive_uniqueify(0, nbytes + 128, true)) {
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
    return 0;
}


Packet *
Packet::shift_data(int offset, bool free_on_failure)
{
  if (offset == 0)
    return this;
  else if (!shared() && (offset < 0 ? headroom() >= (uint32_t)(-offset) : tailroom() >= (uint32_t)offset)) {
    WritablePacket *q = static_cast<WritablePacket *>(this);
    memmove(q->data() + offset, q->data(), q->length());
#if CLICK_LINUXMODULE
    struct sk_buff *mskb = q->skb();
    mskb->data += offset;
    mskb->tail += offset;
#else				/* User-space and BSD kernel module */
    q->_data += offset;
    q->_tail += offset;
# if CLICK_BSDMODULE
    q->m()->m_data += offset;
# endif
#endif
    shift_header_annotations(offset);
    return this;
  } else {
    int tailroom_offset = (offset < 0 ? -offset : 0);
    if (offset < 0 && headroom() < (uint32_t)(-offset))
      offset = -headroom() + ((uintptr_t)(data() + offset) & 7);
    else
      offset += ((uintptr_t)buffer_data() & 7);
    return expensive_uniqueify(offset, tailroom_offset, free_on_failure);
  }
}

CLICK_ENDDECLS
