#ifndef CLICK_NETMAPINFO_HH
#define CLICK_NETMAPINFO_HH 1
#if HAVE_NET_NETMAP_H
#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_user.h>
#include <click/packet.hh>
#include <click/error.hh>
CLICK_DECLS

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
	    --ring->reserved;
	    return true;
	} else
	    return false;
    }

};

CLICK_ENDDECLS
#endif
#endif
