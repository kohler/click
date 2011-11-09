// -*- related-file-name: "../include/click/packet.hh" -*-
/*
 * packet.{cc,hh} -- a packet structure. In the Linux kernel, a synonym for
 * `struct sk_buff'
 * Eddie Kohler, Robert Morris, Nickolai Zeldovich
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2008-2011 Regents of the University of California
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
#include <click/packet_anno.hh>
#include <click/glue.hh>
#include <click/sync.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

/** @file packet.hh
 * @brief The Packet class models packets in Click.
 */

/** @class Packet
 * @brief A network packet.
 * @nosubgrouping
 *
 * Click's Packet class represents network packets within a router.  Packet
 * objects are passed from Element to Element via the Element::push() and
 * Element::pull() functions.  The vast majority of elements handle packets.
 *
 * A packet consists of a <em>data buffer</em>, which stores the actual packet
 * wire data, and a set of <em>annotations</em>, which store extra information
 * calculated about the packet, such as the destination address to be used for
 * routing.  Every Packet object has different annotations, but a data buffer
 * may be shared among multiple Packet objects, saving memory and speeding up
 * packet copies.  (See Packet::clone.)  As a result a Packet's data buffer is
 * not writable.  To write into a packet, turn it into a nonshared
 * WritablePacket first, using uniqueify(), push(), or put().
 *
 * <h3>Data Buffer</h3>
 *
 * A packet's data buffer is a single flat array of bytes.  The buffer may be
 * larger than the actual packet data, leaving unused spaces called
 * <em>headroom</em> and <em>tailroom</em> before and after the data proper.
 * Prepending headers or appending data to a packet can be quite efficient if
 * there is enough headroom or tailroom.
 *
 * The relationships among a Packet object's data buffer variables is shown
 * here:
 *
 * <pre>
 *                     data()               end_data()
 *                        |                      |
 *       |<- headroom() ->|<----- length() ----->|<- tailroom() ->|
 *       |                v                      v                |
 *       +================+======================+================+
 *       |XXXXXXXXXXXXXXXX|   PACKET CONTENTS    |XXXXXXXXXXXXXXXX|
 *       +================+======================+================+
 *       ^                                                        ^
 *       |<------------------ buffer_length() ------------------->|
 *       |                                                        |
 *    buffer()                                               end_buffer()
 * </pre>
 *
 * Most code that manipulates packets is interested only in data() and
 * length().
 *
 * To create a Packet, call one of the make() functions.  To destroy a Packet,
 * call kill().  To clone a Packet, which creates a new Packet object that
 * shares this packet's data, call clone().  To uniqueify a Packet, which
 * unshares the packet data if necessary, call uniqueify().  To allocate extra
 * space for headers or trailers, call push() and put().  To remove headers or
 * trailers, call pull() and take().
 *
 * <pre>
 *                data()                          end_data()
 *                   |                                |
 *           push()  |  pull()                take()  |  put()
 *          <======= | =======>              <======= | =======>
 *                   v                                v
 *       +===========+================================+===========+
 *       |XXXXXXXXXXX|        PACKET CONTENTS         |XXXXXXXXXXX|
 *       +===========+================================+===========+
 * </pre>
 *
 * Packet objects are implemented in different ways in different drivers.  The
 * userlevel driver has its own C++ implementation.  In the linuxmodule
 * driver, however, Packet is an overlay on Linux's native sk_buff
 * object: the Packet methods access underlying sk_buff data directly, with no
 * overhead.  (For example, Packet::data() returns the sk_buff's data field.)
 *
 * <h3>Annotations</h3>
 *
 * Annotations are extra information about a packet above and beyond the
 * packet data.  Packet supports several specific annotations, plus a <em>user
 * annotation area</em> available for arbitrary use by elements.
 *
 * <ul>
 * <li><b>Header pointers:</b> Each packet has three header pointers, designed
 * to point to the packet's MAC header, network header, and transport header,
 * respectively.  Convenience functions like ip_header() access these pointers
 * cast to common header types.  The header pointers are kept up to date when
 * operations like push() or uniqueify() change the packet's data buffer.
 * Header pointers can be null, and they can even point to memory outside the
 * current packet data bounds.  For example, a MAC header pointer will remain
 * set even after pull() is used to shift the packet data past the MAC header.
 * As a result, functions like mac_header_offset() can return negative
 * numbers.</li>
 * <li><b>Timestamp:</b> A timestamp associated with the packet.  Most packet
 * sources timestamp packets when they enter the router; other elements
 * examine or modify the timestamp.</li>
 * <li><b>Device:</b> A pointer to the device on which the packet arrived.
 * Only meaningful in the linuxmodule driver, but provided in every
 * driver.</li>
 * <li><b>Packet type:</b> A small integer indicating whether the packet is
 * meant for this host, broadcast, multicast, or some other purpose.  Several
 * elements manipulate this annotation; in linuxmodule, setting the annotation
 * is required for the host network stack to process incoming packets
 * correctly.</li>
 * <li><b>Performance counter</b> (linuxmodule only): A 64-bit integer
 * intended to hold a performance counter value.  Used by SetCycleCount and
 * others.</li>
 * <li><b>Next and previous packet:</b> Pointers provided to allow elements to
 * chain packets into a doubly linked list.</li>
 * <li><b>Annotations:</b> Each packet has @link Packet::anno_size anno_size
 * @endlink bytes available for annotations.  Elements agree to use portions
 * of the annotation area to communicate per-packet information.  Macros in
 * the <click/packet_anno.hh> header file define the annotations used by
 * Click's current elements.  One common annotation is the network address
 * annotation -- see Packet::dst_ip_anno().  Routing elements, such as
 * RadixIPLookup, set the address annotation to indicate the desired next hop;
 * ARPQuerier uses this annotation to query the next hop's MAC.</li>
 * </ul>
 *
 * New packets start wth all annotations set to zero or null.  Cloning a
 * packet copies its annotations.
 */

/** @class WritablePacket
 * @brief A network packet believed not to be shared.
 *
 * The WritablePacket type represents Packet objects whose data buffers are
 * not shared.  As a result, WritablePacket's versions of functions that
 * access the packet data buffer, such as data(), end_buffer(), and
 * ip_header(), return mutable pointers (<tt>char *</tt> rather than <tt>const
 * char *</tt>).
 *
 * WritablePacket objects are created by Packet::make(), Packet::uniqueify(),
 * Packet::push(), and Packet::put(), which ensure that the returned packet
 * does not share its data buffer.
 *
 * WritablePacket's interface is the same as Packet's except for these type
 * differences.  For documentation, see Packet.
 *
 * @warning The WritablePacket convention reduces the likelihood of error
 * when modifying packet data, but does not eliminate it.  For instance, by
 * calling WritablePacket::clone(), it is possible to create a WritablePacket
 * whose data is shared:
 * @code
 * Packet *p = ...;
 * if (WritablePacket *q = p->uniqueify()) {
 *     Packet *p2 = q->clone();
 *     assert(p2);
 *     q->ip_header()->ip_v = 6;   // modifies p2's data as well
 * }
 * @endcode
 * Avoid writing buggy code like this!  Use WritablePacket selectively, and
 * try to avoid calling WritablePacket::clone() when possible. */

Packet::~Packet()
{
    // This is a convenient place to put static assertions.
    static_assert(addr_anno_offset % 8 == 0 && user_anno_offset % 8 == 0,
		  "Annotations must begin at multiples of 8 bytes.");
    static_assert(addr_anno_offset + addr_anno_size <= anno_size,
		  "Annotation area too small for address annotations.");
    static_assert(user_anno_offset + user_anno_size <= anno_size,
		  "Annotation area too small for user annotations.");
    static_assert(dst_ip_anno_offset == DST_IP_ANNO_OFFSET
		  && dst_ip6_anno_offset == DST_IP6_ANNO_OFFSET
		  && dst_ip_anno_size == DST_IP_ANNO_SIZE
		  && dst_ip6_anno_size == DST_IP6_ANNO_SIZE
		  && dst_ip_anno_size == 4
		  && dst_ip6_anno_size == 16
		  && dst_ip_anno_offset + 4 <= anno_size
		  && dst_ip6_anno_offset + 16 <= anno_size,
		  "Address annotations at unexpected locations.");
    static_assert((default_headroom & 3) == 0,
		  "Default headroom should be a multiple of 4 bytes.");
#if CLICK_LINUXMODULE
    static_assert(sizeof(Anno) <= sizeof(((struct sk_buff *)0)->cb),
		  "Anno structure too big for Linux packet annotation area.");
#endif

#if CLICK_LINUXMODULE
    panic("Packet destructor");
#else
    if (_data_packet)
	_data_packet->kill();
# if CLICK_USERLEVEL
    else if (_head && _destructor)
	_destructor(_head, _end - _head);
    else
	delete[] _head;
# elif CLICK_BSDMODULE
    if (_m)
	m_freem(_m);
# endif
    _head = _data = 0;
#endif
}

#if !CLICK_LINUXMODULE

# if HAVE_CLICK_PACKET_POOL
#  define CLICK_PACKET_POOL_BUFSIZ		2048
#  define CLICK_PACKET_POOL_SIZE		1000 // see LIMIT in packetpool-01.testie
#  define CLICK_GLOBAL_PACKET_POOL_COUNT	16
namespace {
struct PacketData {
    PacketData *next;
#  if HAVE_MULTITHREAD
    PacketData *pool_next;
#  endif
};
struct PacketPool {
    WritablePacket *p;
    unsigned pcount;
    PacketData *pd;
    unsigned pdcount;
#  if HAVE_MULTITHREAD
    PacketPool *chain;
#  endif
};
}
#  if HAVE_MULTITHREAD
static __thread PacketPool *thread_packet_pool;
static PacketPool *all_thread_packet_pools;
static PacketPool global_packet_pool;
static volatile uint32_t global_packet_pool_lock;

static inline PacketPool *
get_packet_pool()
{
    PacketPool *pp = thread_packet_pool;
    if (!pp && (pp = new PacketPool)) {
	memset(pp, 0, sizeof(PacketPool));
	while (atomic_uint32_t::swap(global_packet_pool_lock, 1) == 1)
	    /* do nothing */;
	pp->chain = all_thread_packet_pools;
	all_thread_packet_pools = pp;
	thread_packet_pool = pp;
	click_compiler_fence();
	global_packet_pool_lock = 0;
    }
    return pp;
}
#  else
static PacketPool packet_pool;
#  endif

WritablePacket *
WritablePacket::pool_allocate(bool with_data)
{
#  if HAVE_MULTITHREAD
    PacketPool &packet_pool = *get_packet_pool();
    if ((!packet_pool.p && global_packet_pool.p)
	|| (with_data && !packet_pool.pd && global_packet_pool.pd)) {
	while (atomic_uint32_t::swap(global_packet_pool_lock, 1) == 1)
	    /* do nothing */;

	WritablePacket *pp;
	if (!packet_pool.p && (pp = global_packet_pool.p)) {
	    global_packet_pool.p = static_cast<WritablePacket *>(pp->prev());
	    --global_packet_pool.pcount;
	    packet_pool.p = pp;
	    packet_pool.pcount = CLICK_PACKET_POOL_SIZE;
	}

	PacketData *pd;
	if (with_data && !packet_pool.pd && (pd = global_packet_pool.pd)) {
	    global_packet_pool.pd = pd->pool_next;
	    --global_packet_pool.pdcount;
	    packet_pool.pd = pd;
	    packet_pool.pdcount = CLICK_PACKET_POOL_SIZE;
	}

	click_compiler_fence();
	global_packet_pool_lock = 0;
    }
#  else
    (void) with_data;
#  endif

    WritablePacket *p = packet_pool.p;
    if (p) {
	packet_pool.p = static_cast<WritablePacket *>(p->next());
	--packet_pool.pcount;
    } else
	p = new WritablePacket;
    return p;
}

WritablePacket *
WritablePacket::pool_allocate(uint32_t headroom, uint32_t length,
			      uint32_t tailroom)
{
    uint32_t n = headroom + length + tailroom;
    if (n < CLICK_PACKET_POOL_BUFSIZ)
	n = CLICK_PACKET_POOL_BUFSIZ;
    WritablePacket *p = pool_allocate(n == CLICK_PACKET_POOL_BUFSIZ);
    if (p) {
	p->initialize();
	PacketData *pd;
#  if HAVE_MULTITHREAD
	PacketPool &packet_pool = *thread_packet_pool;
#  endif
	if (n == CLICK_PACKET_POOL_BUFSIZ && (pd = packet_pool.pd)) {
	    packet_pool.pd = pd->next;
	    --packet_pool.pdcount;
	    p->_head = reinterpret_cast<unsigned char *>(pd);
	} else if ((p->_head = new unsigned char[n]))
	    /* OK */;
	else {
	    delete p;
	    return 0;
	}
	p->_data = p->_head + headroom;
	p->_tail = p->_data + length;
	p->_end = p->_head + n;
    }
    return p;
}

void
WritablePacket::recycle(WritablePacket *p)
{
    unsigned char *data = 0;
    if (!p->_data_packet && p->_head && !p->_destructor
	&& p->_end - p->_head == CLICK_PACKET_POOL_BUFSIZ) {
	data = p->_head;
	p->_head = 0;
    }
    p->~WritablePacket();

#  if HAVE_MULTITHREAD
    PacketPool &packet_pool = *get_packet_pool();
    if ((packet_pool.p && packet_pool.pcount == CLICK_PACKET_POOL_SIZE)
	|| (data && packet_pool.pd && packet_pool.pdcount == CLICK_PACKET_POOL_SIZE)) {
	while (atomic_uint32_t::swap(global_packet_pool_lock, 1) == 1)
	    /* do nothing */;

	if (packet_pool.p && packet_pool.pcount == CLICK_PACKET_POOL_SIZE) {
	    if (global_packet_pool.pcount == CLICK_GLOBAL_PACKET_POOL_COUNT) {
		while (WritablePacket *p = packet_pool.p) {
		    packet_pool.p = static_cast<WritablePacket *>(p->next());
		    ::operator delete((void *) p);
		}
	    } else {
		packet_pool.p->set_prev(global_packet_pool.p);
		global_packet_pool.p = packet_pool.p;
		++global_packet_pool.pcount;
		packet_pool.p = 0;
	    }
	    packet_pool.pcount = 0;
	}

	if (data && packet_pool.pd && packet_pool.pdcount == CLICK_PACKET_POOL_SIZE) {
	    if (global_packet_pool.pdcount == CLICK_GLOBAL_PACKET_POOL_COUNT) {
		while (PacketData *pd = packet_pool.pd) {
		    packet_pool.pd = pd->next;
		    delete[] reinterpret_cast<unsigned char *>(pd);
		}
	    } else {
		packet_pool.pd->pool_next = global_packet_pool.pd;
		global_packet_pool.pd = packet_pool.pd;
		++global_packet_pool.pdcount;
		packet_pool.pd = 0;
	    }
	    packet_pool.pdcount = 0;
	}

	click_compiler_fence();
	global_packet_pool_lock = 0;
    }
#  else
    if (packet_pool.pcount == CLICK_PACKET_POOL_SIZE) {
	::operator delete((void *) p);
	p = 0;
    }
    if (data && packet_pool.pdcount == CLICK_PACKET_POOL_SIZE) {
	delete[] data;
	data = 0;
    }
#  endif

    if (p) {
	++packet_pool.pcount;
	p->set_next(packet_pool.p);
	packet_pool.p = p;
	assert(packet_pool.pcount <= CLICK_PACKET_POOL_SIZE);
    }
    if (data) {
	++packet_pool.pdcount;
	PacketData *pd = reinterpret_cast<PacketData *>(data);
	pd->next = packet_pool.pd;
	packet_pool.pd = pd;
	assert(packet_pool.pdcount <= CLICK_PACKET_POOL_SIZE);
    }
}

#endif

bool
Packet::alloc_data(uint32_t headroom, uint32_t length, uint32_t tailroom)
{
    uint32_t n = length + headroom + tailroom;
    if (n < min_buffer_length) {
	tailroom = min_buffer_length - length - headroom;
	n = min_buffer_length;
    }
#if CLICK_USERLEVEL
    unsigned char *d = new unsigned char[n];
    if (!d)
	return false;
    _head = d;
    _data = d + headroom;
    _tail = _data + length;
    _end = _head + n;
#elif CLICK_BSDMODULE
    //click_chatter("allocate new mbuf, length=%d", n);
    if (n > MJUM16BYTES) {
	click_chatter("trying to allocate %d bytes: too many\n", n);
	return false;
    }
    struct mbuf *m;
    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (!m)
	return false;
    if (n > MHLEN) {
	if (n > MCLBYTES)
	    m_cljget(m, M_DONTWAIT, (n <= MJUMPAGESIZE ? MJUMPAGESIZE :
				     n <= MJUM9BYTES   ? MJUM9BYTES   :
 							 MJUM16BYTES));
	else
	    MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
	    m_freem(m);
	    return false;
	}
    }
    _m = m;
    _m->m_data += headroom;
    _m->m_len = length;
    _m->m_pkthdr.len = length;
    assimilate_mbuf();
#endif
    return true;
}

#endif

/** @brief Create and return a new packet.
 * @param headroom headroom in new packet
 * @param data data to be copied into the new packet
 * @param length length of packet
 * @param tailroom tailroom in new packet
 * @return new packet, or null if no packet could be created
 *
 * The @a data is copied into the new packet.  If @a data is null, the
 * packet's data is left uninitialized.  The resulting packet's
 * buffer_length() will be at least @link Packet::min_buffer_length
 * min_buffer_length @endlink; if @a headroom + @a length + @a tailroom would
 * be less, then @a tailroom is increased to make the total @link
 * Packet::min_buffer_length min_buffer_length @endlink.
 *
 * The new packet's annotations are cleared and its header pointers are
 * null. */
WritablePacket *
Packet::make(uint32_t headroom, const void *data,
	     uint32_t length, uint32_t tailroom)
{
#if CLICK_LINUXMODULE
    int want = 1;
    if (struct sk_buff *skb = skbmgr_allocate_skbs(headroom, length + tailroom, &want)) {
	assert(want == 1);
	// packet comes back from skbmgr with headroom reserved
	__skb_put(skb, length);	// leave space for data
	if (data)
	    memcpy(skb->data, data, length);
# if PACKET_CLEAN
	skb->pkt_type = HOST | PACKET_CLEAN;
# else
	skb->pkt_type = HOST;
# endif
	WritablePacket *q = reinterpret_cast<WritablePacket *>(skb);
	q->clear_annotations();
	return q;
    } else
	return 0;
#else
# if HAVE_CLICK_PACKET_POOL
    WritablePacket *p = WritablePacket::pool_allocate(headroom, length, tailroom);
    if (!p)
	return 0;
# else
    WritablePacket *p = new WritablePacket;
    if (!p)
	return 0;
    p->initialize();
    if (!p->alloc_data(headroom, length, tailroom)) {
	p->_head = 0;
	delete p;
	return 0;
    }
# endif
    if (data)
	memcpy(p->data(), data, length);
    return p;
#endif
}

#if CLICK_USERLEVEL
/** @brief Create and return a new packet (userlevel).
 * @param data data used in the new packet
 * @param length length of packet
 * @param destructor destructor function
 * @return new packet, or null if no packet could be created
 *
 * The packet's data pointer becomes the @a data: the data is not copied into
 * the new packet, rather the packet owns the @a data pointer.  When the
 * packet's data is eventually destroyed, either because the packet is deleted
 * or because of something like a push() or full(), the @a destructor will be
 * called with arguments @a destructor(@a data, @a length).  (If @a destructor
 * is null, the packet data will be freed by <tt>delete[] @a data</tt>.)  The
 * packet has zero headroom and tailroom.
 *
 * The returned packet's annotations are cleared and its header pointers are
 * null. */
WritablePacket *
Packet::make(unsigned char *data, uint32_t length,
	     void (*destructor)(unsigned char *, size_t))
{
# if HAVE_CLICK_PACKET_POOL
    WritablePacket *p = WritablePacket::pool_allocate(false);
# else
    WritablePacket *p = new WritablePacket;
# endif
    if (p) {
	p->initialize();
	p->_head = p->_data = data;
	p->_tail = p->_end = data + length;
	p->_destructor = destructor;
    }
    return p;
}
#endif


//
// UNIQUEIFICATION
//

/** @brief Create a clone of this packet.
 * @return the cloned packet
 *
 * The returned clone has independent annotations, initially copied from this
 * packet, but shares this packet's data.  shared() returns true for both the
 * packet and its clone.  Returns null if there's no memory for the clone. */
Packet *
Packet::clone()
{
#if CLICK_LINUXMODULE

    struct sk_buff *nskb = skb_clone(skb(), GFP_ATOMIC);
    return reinterpret_cast<Packet *>(nskb);

#elif CLICK_USERLEVEL || CLICK_BSDMODULE
# if CLICK_BSDMODULE
    struct mbuf *m;

    if (this->_m == NULL)
        return 0;

    if (this->_m->m_flags & M_EXT
        && (   this->_m->m_ext.ext_type == EXT_JUMBOP
            || this->_m->m_ext.ext_type == EXT_JUMBO9
            || this->_m->m_ext.ext_type == EXT_JUMBO16)) {
        if ((m = dup_jumbo_m(this->_m)) == NULL)
	    return 0;
    }
    else if ((m = m_dup(this->_m, M_DONTWAIT)) == NULL)
	return 0;
# endif

    // timing: .31-.39 normal, .43-.55 two allocs, .55-.58 two memcpys
# if HAVE_CLICK_PACKET_POOL
    Packet *p = WritablePacket::pool_allocate(false);
# else
    Packet *p = new WritablePacket; // no initialization
# endif
    if (!p)
	return 0;
    memcpy(p, this, sizeof(Packet));
    p->_use_count = 1;
    p->_data_packet = this;
# if CLICK_USERLEVEL
    p->_destructor = 0;
# else
    p->_m = m;
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
    size = SKB_DATA_ALIGN(size);
    unsigned char *new_head = reinterpret_cast<unsigned char *>(kmalloc(size + sizeof(struct skb_shared_info), GFP_ATOMIC));
    if (!new_head) {
	if (free_on_failure)
	    kill();
	return 0;
    }

    unsigned char *start_copy = old_head + (extra_headroom >= 0 ? 0 : -extra_headroom);
    unsigned char *end_copy = old_head + buffer_length() + (extra_tailroom >= 0 ? 0 : extra_tailroom);
    memcpy(new_head + (extra_headroom >= 0 ? extra_headroom : 0), start_copy, end_copy - start_copy);

    if (!nskb->cloned || atomic_dec_and_test(&(skb_shinfo(nskb)->dataref))) {
	assert(!skb_shinfo(nskb)->nr_frags && !skb_shinfo(nskb)->frag_list);
	kfree(old_head);
    }

    nskb->head = new_head;
    nskb->data = new_head + old_headroom + extra_headroom;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    skb_set_tail_pointer(nskb, old_length);
# else
    nskb->tail = nskb->data + old_length;
# endif
# ifdef NET_SKBUFF_DATA_USES_OFFSET
    nskb->end = size;
# else
    nskb->end = new_head + size;
# endif
    nskb->len = old_length;
    nskb->cloned = 0;

    nskb->truesize = size + sizeof(struct sk_buff);
    struct skb_shared_info *nskb_shinfo = skb_shinfo(nskb);
    atomic_set(&nskb_shinfo->dataref, 1);
    nskb_shinfo->nr_frags = 0;
    nskb_shinfo->frag_list = 0;
# if HAVE_LINUX_SKB_SHINFO_GSO_SIZE
    nskb_shinfo->gso_size = 0;
    nskb_shinfo->gso_segs = 0;
    nskb_shinfo->gso_type = 0;
# elif HAVE_LINUX_SKB_SHINFO_TSO_SIZE
    nskb_shinfo->tso_size = 0;
    nskb_shinfo->tso_segs = 0;
# endif
# if HAVE_LINUX_SKB_SHINFO_UFO_SIZE
    nskb_shinfo->ufo_size = 0;
# endif
# if HAVE_LINUX_SKB_SHINFO_IP6_FRAG_ID
    nskb_shinfo->ip6_frag_id = 0;
# endif

    shift_header_annotations(old_head, extra_headroom);
    return static_cast<WritablePacket *>(this);

#else /* !CLICK_LINUXMODULE */

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
    m_freem(old_m); // alloc_data() created a new mbuf, so free the old one
# endif

    _use_count = 1;
    _data_packet = 0;
    shift_header_annotations(old_head, extra_headroom);
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

/*
 * Duplicate a packet by copying data from an mbuf chain to a new mbuf with a
 * jumbo cluster (i.e., contiguous storage).
 */
struct mbuf *
Packet::dup_jumbo_m(struct mbuf *m)
{
  int len = m->m_pkthdr.len;
  struct mbuf *new_m;

  if (len > MJUM16BYTES) {
    click_chatter("warning: cannot allocate jumbo cluster for %d bytes", len);
    return NULL;
  }

  new_m = m_getjcl(M_DONTWAIT, m->m_type, m->m_flags & M_COPYFLAGS,
                   (len <= MJUMPAGESIZE ? MJUMPAGESIZE :
                    len <= MJUM9BYTES   ? MJUM9BYTES   :
                                          MJUM16BYTES));
  if (!new_m) {
    click_chatter("warning: jumbo cluster mbuf allocation failed");
    return NULL;
  }

  m_copydata(m, 0, len, mtod(new_m, caddr_t));
  new_m->m_len = len;
  new_m->m_pkthdr.len = len;

  /* XXX: Only a subset of what m_dup_pkthdr() would copy: */
  new_m->m_pkthdr.rcvif = m->m_pkthdr.rcvif;
# if __FreeBSD_version >= 800000
  new_m->m_pkthdr.flowid = m->m_pkthdr.flowid;
# endif
  new_m->m_pkthdr.ether_vtag = m->m_pkthdr.ether_vtag;

  return new_m;
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

    // Preserve mac_header, network_header, and transport_header.
    const unsigned char *dp = data();
    if (has_mac_header() && mac_header() >= buffer()
	&& mac_header() <= end_buffer() && mac_header() < dp)
	dp = mac_header();
    if (has_network_header() && network_header() >= buffer()
	&& network_header() <= end_buffer() && network_header() < dp)
	dp = network_header();
    if (has_transport_header() && transport_header() >= buffer()
	&& transport_header() <= end_buffer() && transport_header() < dp)
	dp = network_header();

    if (!shared()
	&& (offset < 0 ? (dp - buffer()) >= (ptrdiff_t)(-offset)
	    : tailroom() >= (uint32_t)offset)) {
	WritablePacket *q = static_cast<WritablePacket *>(this);
	memmove((unsigned char *) dp + offset, dp, q->end_data() - dp);
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
	shift_header_annotations(q->buffer(), offset);
	return this;
    } else {
	int tailroom_offset = (offset < 0 ? -offset : 0);
	if (offset < 0 && headroom() < (uint32_t)(-offset))
	    offset = -headroom() + ((uintptr_t)(data() + offset) & 7);
	else
	    offset += ((uintptr_t)buffer() & 7);
	return expensive_uniqueify(offset, tailroom_offset, free_on_failure);
    }
}


#if HAVE_CLICK_PACKET_POOL
static void
cleanup_pool(PacketPool *pp, int global)
{
    unsigned pcount = 0, pdcount = 0;
    while (WritablePacket *p = pp->p) {
	++pcount;
	pp->p = static_cast<WritablePacket *>(p->next());
	::operator delete((void *) p);
    }
    while (PacketData *pd = pp->pd) {
	++pdcount;
	pp->pd = pd->next;
	delete[] reinterpret_cast<unsigned char *>(pd);
    }
    assert(pcount <= CLICK_PACKET_POOL_SIZE);
    assert(pdcount <= CLICK_PACKET_POOL_SIZE);
    assert(global || (pcount == pp->pcount && pdcount == pp->pdcount));
}
#endif

void
Packet::static_cleanup()
{
#if HAVE_CLICK_PACKET_POOL
# if HAVE_MULTITHREAD
    while (PacketPool *pp = all_thread_packet_pools) {
	all_thread_packet_pools = pp->chain;
	cleanup_pool(pp, 0);
	delete pp;
    }
    unsigned rounds = (global_packet_pool.pcount > global_packet_pool.pdcount ? global_packet_pool.pcount : global_packet_pool.pdcount);
    assert(rounds <= CLICK_GLOBAL_PACKET_POOL_COUNT);
    while (global_packet_pool.p || global_packet_pool.pd) {
	WritablePacket *next_p = global_packet_pool.p;
	next_p = (next_p ? static_cast<WritablePacket *>(next_p->prev()) : 0);
	PacketData *next_pd = global_packet_pool.pd;
	next_pd = (next_pd ? next_pd->pool_next : 0);
	cleanup_pool(&global_packet_pool, 1);
	global_packet_pool.p = next_p;
	global_packet_pool.pd = next_pd;
	--rounds;
    }
    assert(rounds == 0);
# else
    cleanup_pool(&packet_pool, 0);
# endif
#endif
}

CLICK_ENDDECLS
