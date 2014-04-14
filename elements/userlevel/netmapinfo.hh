#ifndef CLICK_NETMAPINFO_HH
#define CLICK_NETMAPINFO_HH 1
#if HAVE_NET_NETMAP_H
#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_user.h>
#include <click/packet.hh>
#include <click/error.hh>
CLICK_DECLS

#if NETMAP_API > 9
	#define NETMAP_RING_FIRST_RESERVED(r)                   \
		((r)->head)
	#define NETMAP_RING_NEXT nm_ring_next
#else
	static inline int nm_ring_space(struct netmap_ring* ring) {
		return ring->avail;
	}
#endif

class NetmapInfo { public:

    struct ring {
	char *mem;
	unsigned ring_begin;
	unsigned ring_end;
	struct netmap_if *nifp;

	int open(const String &ifname,
		 bool always_error, ErrorHandler *errh);
	void initialize_rings_rx(int timestamp);
	void initialize_rings_tx();
	void close(int fd);
    };

    static unsigned char *buffers;	// XXX not thread safe
    static bool is_netmap_buffer(Packet *p) {
	return p->buffer_destructor() == buffer_destructor;
    }
    static void buffer_destructor(unsigned char *buf, size_t, void*) {
	*reinterpret_cast<unsigned char **>(buf) = buffers;
	buffers = buf;
    }
    static bool refill(struct netmap_ring *ring) {
	if (buffers) {
	    unsigned char *buf = buffers;
	    buffers = *reinterpret_cast<unsigned char **>(buffers);
	    unsigned res1idx = NETMAP_RING_FIRST_RESERVED(ring);
	    ring->slot[res1idx].buf_idx = NETMAP_BUF_IDX(ring, (char *) buf);
	    ring->slot[res1idx].flags |= NS_BUF_CHANGED;
	#if NETMAP_API < 10
	    --ring->reserved;
	#else
	    ring->head++;
	    if (ring->head == ring->num_slots) ring->head = 0;
	#endif
	    return true;
	} else
	    return false;
    }

    static inline int nm_ring_reserved(struct netmap_ring *ring) {
	#if NETMAP_API < 10
    	return ring->reserved;
	#else
    	int ret = ring->cur - ring->head;
    	if (ret < 0)
    		ret += ring->num_slots;
    	return ret;
	#endif
    }

    static inline bool nm_ring_has_avail(struct netmap_ring *ring) {
	#if NETMAP_API < 10
    	return ring->avail > 0;
	#else
    	return ring->tail != ring->cur;
	#endif
    }

    static inline int nm_advance(struct netmap_ring *ring,int nzcopy) {
    	ring->cur = NETMAP_RING_NEXT(ring, ring->cur);
	#if NETMAP_API < 10
    	--ring->avail;
    	if (nzcopy > 0) {
    		++ring->reserved;
    		return nzcopy - 1;
    	} else {
    		return 0;
    	}

	#else
    	if (nzcopy > 0) {
    		return nzcopy - 1;
    	} else {
    		ring->head++;
    		if (ring->head == ring->num_slots) ring->head = 0;
    		return 0;
    	}
	#endif
    }

    static inline void nm_send_pkt(struct netmap_ring *ring) {
    	ring->cur = NETMAP_RING_NEXT(ring, ring->cur);
	#if NETMAP_API < 10
    	--ring->avail;
	#else
    	ring->head = ring->cur;
	#endif
    }


};

CLICK_ENDDECLS
#endif
#endif
