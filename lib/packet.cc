#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "packet.hh"
#include "glue.hh"
#include <assert.h>

#ifdef __KERNEL__

Packet::Packet()
{
  panic("Packet constructor");
}

Packet::~Packet()
{
  panic("Packet destructor");
}

Packet *
Packet::make(const unsigned char *data, unsigned len)
{
  unsigned headroom = 24;
  unsigned tailroom = 0;
  if (len < 64)
    tailroom = 64 - len;
  if(tailroom < 8)
    tailroom = 8;
  int size = len + headroom + tailroom;
  struct sk_buff *skb = alloc_skb(size, GFP_ATOMIC);
  if (skb) {
    skb_reserve(skb, headroom);	// leave some headroom
    skb_put(skb, len);		// leave space for data
    if (data) memcpy(skb->data, data, len);
  }
  Packet *p = (Packet *) skb;
  p->set_mac_broadcast_anno(0);
  p->set_fix_ip_src_anno(0);
  p->set_color_anno(0);
  return(p);
}

#else /* __KERNEL__ */

inline
Packet::Packet()
{
  _use_count = 1;
  _data_packet = 0;
  _head = _data = _tail = _end = 0;
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

inline Packet *
Packet::make(int, int, int)
{
  return new Packet(6, 6, 6);
}

void
Packet::alloc_data(int len)
{
  int head_slop = 64;
  int tail_slop = 0;
  
  if (len < 64)
    tail_slop = 64 - len;
  if(tail_slop < 8)
    tail_slop = 8;
  int n = len + head_slop + tail_slop;
  
  _head = new unsigned char[n];
  _data = _head + head_slop;
  _tail = _data + len;
  _end = _head + n;
}

Packet *
Packet::make(const unsigned char *data, unsigned len)
{
  Packet *p = new Packet;
  if (p) {
    p->alloc_data(len);
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

Packet *
Packet::uniqueify_copy()
{
  struct sk_buff *n = skb_copy(skb(), GFP_ATOMIC);
  kill();
  return (Packet *)n;
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
  memcpy(p->_cb, _cb, sizeof(_cb));
  // increment our reference count because of _data_packet reference
  _use_count++;
  return p;
}

Packet *
Packet::uniqueify_copy()
{
  Packet *p = Packet::make(6, 6, 6); // dummy arguments: no initialization
  if (!p) return 0;
  p->_use_count = 1;
  p->_data_packet = 0;
  p->alloc_data(_end - _head);
  memcpy(p->_head, _head, _end - _head);
  p->_data = p->_head + (_data - _head);
  p->_tail = p->_head + (_tail - _head);
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
 * Creates and returns a new packet if the packet
 * has other references.
 * May return the same packet if it has only one reference.
 */
Packet *
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
  } else {
    click_chatter("expensive Packet::push");
#ifdef __KERNEL__
    Packet *q = Packet::make(skb_realloc_headroom(skb(), nbytes));
#else
    Packet *q = Packet::make(length() + nbytes);
    memcpy(q->data() + nbytes, data(), length());
    memcpy(q->anno(), anno(), sizeof(Anno));
#endif
    kill();
    return q;
  }
}

Packet *
Packet::put(unsigned int nbytes)
{
  if (tailroom() >= nbytes) {
    Packet *p = uniqueify();
#ifdef __KERNEL__
    skb_put(p->skb(), nbytes);
#else
    p->_tail += nbytes;
#endif
    return p;
  } else {
    click_chatter("expensive Packet::put");
#ifdef __KERNEL__
    Packet *q = 0;
    panic("Packet::put");
#else
    Packet *q = Packet::make(length() + nbytes);
    memcpy(q->data(), data(), length());
    memcpy(q->anno(), anno(), sizeof(Anno));
#endif
    kill();
    return q;
  }
}

Packet *
Packet::take(unsigned int nbytes)
{
  if(nbytes <= length()){
    Packet *p = uniqueify();
#ifdef __KERNEL__
    skb()->tail -= nbytes;
    skb()->len -= nbytes;
#else
    p->_tail -= nbytes;
#endif    
    return(p);
  } else {
    click_chatter("Packet::take oops");
    return(this);
  }
}
