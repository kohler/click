#ifndef CLICK_NETMAPINFO_HH
#define CLICK_NETMAPINFO_HH 1

#if HAVE_NET_NETMAP_H
#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_user.h>

// XXX bug in netmap_user.h , the prototype should be available

#ifndef NETMAP_WITH_LIBS
typedef void (*nm_cb_t)(u_char *, const struct nm_pkthdr *, const u_char *d);
#endif

#include <click/packet.hh>
#include <click/error.hh>
CLICK_DECLS

/* a queue of netmap buffers, by index */
class NetmapBufQ {
    unsigned char *buf_start;	/* base address */
    unsigned int buf_size;
    unsigned int max_index;	/* error checking */
    unsigned char *buf_end; /* error checking */

    unsigned int head;	/* index of first buffer */
    unsigned int tail;	/* index of last buffer */
    unsigned int count;	/* how many ? */

  public:
    inline unsigned int insert(unsigned int idx) {
	if (idx >= max_index) {
	    return 1; // error
	}
	unsigned int *p = reinterpret_cast<unsigned int *>(buf_start +
		idx * buf_size);
	// prepend
	*p = head;
	if (head == 0) {
	    tail = idx;
	}
	head = idx;
	count++;
	return 0;
    }
    inline unsigned int insert_p(unsigned char *p) {
	if (p < buf_start || p >= buf_end)
	    return 1;
	return insert((p - buf_start) / buf_size);
    }
    inline unsigned int extract() {
	if (count == 0)
	    return 0;
	unsigned int idx = head;
	unsigned int *p = reinterpret_cast<unsigned int *>(buf_start +
		idx * buf_size);
	head = *p;
	count--;
	return idx;
    }
    inline unsigned char * extract_p() {
	unsigned int idx = extract();
	return (idx == 0) ? 0 : buf_start + idx * buf_size;
    }
    inline int init (void *beg, void *end, uint32_t _size) {
	click_chatter("Initializing NetmapBufQ %p size %d mem %p %p\n",
		this, _size, beg, end);
	head = tail = max_index = 0;
	count = 0;
	buf_size = 0;
	buf_start = buf_end = 0;
	if (_size == 0 || _size > 0x10000 ||
		beg == 0 || end == 0 || end < beg) {
	    click_chatter("NetmapBufQ %p bad args: size %d mem %p %p\n",
		this, _size, beg, end);
	    return 1;
	}
	buf_size = _size;
	buf_start = reinterpret_cast<unsigned char *>(beg);
	buf_end = reinterpret_cast<unsigned char *>(end);
	max_index = (buf_end - buf_start) / buf_size;
	// check max_index overflow ?
	return 0;
    }
};

/* a netmap port as returned by nm_open */
class NetmapInfo { public:

	struct nm_desc *desc;
	class NetmapInfo *parent;	/* same pool */
	class NetmapBufQ bufq;		/* free buffer queue */

	// to recycle buffers,
	// nmr.arg3 is the number of extra buffers
	// nifp->ni_bufs_head is the index of the first buffer.
	unsigned int active_users; // we do not close until done.

	NetmapInfo *destructor_arg;	// either this or parent's main_mem

	int open(const String &ifname,
		 bool always_error, ErrorHandler *errh);
	void initialize_rings_rx(int timestamp);
	void initialize_rings_tx();
	void close(int fd);
	// send a packet, possibly using zerocopy if noutputs == 0
	// and other conditions apply
        bool send_packet(Packet *p, int noutputs);

	int dispatch(int burst, nm_cb_t cb, u_char *arg);

#if 0
	// XXX return a buffer to the ring
	bool refill(struct netmap_ring *ring) {
	    if (buffers) {
		unsigned char *buf = buffers;
		buffers = *reinterpret_cast<unsigned char **>(buffers);
		unsigned res1idx = ring->head;
		ring->slot[res1idx].buf_idx = NETMAP_BUF_IDX(ring, (char *) buf);
		ring->slot[res1idx].flags |= NS_BUF_CHANGED;
		ring->head = nm_ring_next(ring, res1idx);
		return true;
	    } else
		return false;
	}
#endif

    static bool is_netmap_buffer(Packet *p) {
	return p->buffer_destructor() == buffer_destructor;
    }

    /*
     * the destructor appends the buffer to the freelist in the ring,
     * using the first field as pointer.
     */
    static void buffer_destructor(unsigned char *buf, size_t, void *arg) {
	NetmapInfo *x = reinterpret_cast<NetmapInfo *>(arg);
	click_chatter("%s ni %p buf %p\n", __FUNCTION__,
		x, buf);
	if (x->bufq.insert_p(buf)) {
		// XXX error
	}
    }
};

CLICK_ENDDECLS
#endif // HAVE_NETMAP_H

#endif
