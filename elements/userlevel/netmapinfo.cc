// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * netmapinfo.{cc,hh} -- library for interfacing with netmap
 * Eddie Kohler, Luigi Rizzo
 *
 * Copyright (c) 2012 Eddie Kohler
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
#include <click/glue.hh>
#if HAVE_NET_NETMAP_H
#define NETMAP_WITH_LIBS
#include "netmapinfo.hh"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <click/sync.hh>
#include <unistd.h>
#include <fcntl.h>
CLICK_DECLS

/*
 * keep a list of netmap ports so matching the name we
 * can recycle the regions
 */
static Spinlock netmap_memory_lock;
//static struct NetmapInfo *netmap_ports;

int
NetmapInfo::open(const String &ifname,
		       bool always_error, ErrorHandler *errh)
{
    click_chatter("%s ifname %s\n", __FUNCTION__, ifname.c_str());
    ErrorHandler *initial_errh = always_error ? errh : ErrorHandler::silent_handler();

    netmap_memory_lock.acquire();
    // for the time being, just a new block
    do {
	desc = nm_open(ifname.c_str(), NULL, 0, NULL);
	if (desc == NULL) {
	    initial_errh->error("nm_open(%s): %s", ifname.c_str(), strerror(errno));
	    break;
	}
	click_chatter("%s %s memsize %d mem %p buf_start %p buf_end %p",
		__FUNCTION__, desc->req.nr_name,
		desc->memsize, desc->mem, desc->buf_start, desc->buf_end);
	bufq.init(desc->buf_start, desc->buf_end,
		desc->some_ring->nr_buf_size);
	/* eventually try to match the region */
	destructor_arg = this;
	click_chatter("private mapping for %s\n", ifname.c_str());
    } while (0);
    netmap_memory_lock.release();
    return desc ? desc->fd : -1;
}

void
NetmapInfo::initialize_rings_rx(int timestamp)
{
    click_chatter("%s timestamp %d\n", __FUNCTION__, timestamp);
    if (timestamp >= 0) {
	int flags = (timestamp > 0 ? NR_TIMESTAMP : 0);
	for (unsigned i = desc->first_rx_ring; i <= desc->last_rx_ring; ++i)
	    NETMAP_RXRING(desc->nifp, i)->flags = flags;
    }
}

void
NetmapInfo::initialize_rings_tx()
{
    click_chatter("%s\n", __FUNCTION__);
}

int
NetmapInfo::dispatch(int count, nm_cb_t cb, u_char *arg)
{
	return nm_dispatch(desc, count, cb, arg);
}

bool
NetmapInfo::send_packet(Packet *p, int noutputs)
{
	int ret = nm_inject(desc, p->data(), p->length());
	if (0) click_chatter("%s buf %p size %d returns %d\n",
		__FUNCTION__, p->data(), p->length(), ret);
	return ret > 0 ? 0 : -1;
#if 0
    // we can do a smart nm_inject
    for (unsigned ri = desc->first_tx_ring; ri <= desc->last_tx_ring; ++ri) {
        struct netmap_ring *ring = NETMAP_TXRING(desc->nifp, ri);
        if (nm_ring_empty(ring))
            continue;
        unsigned cur = ring->cur;
        unsigned buf_idx = ring->slot[cur].buf_idx;
        if (buf_idx < 2)
            continue;
        unsigned char *buf = (unsigned char *) NETMAP_BUF(ring, buf_idx);
        uint32_t p_length = p->length();
        if (NetmapInfo::is_netmap_buffer(p)
            && !p->shared()
	    && p->buffer() == p->data()
            && (char *)p->buffer() >= desc->buf_start
	    && (char *)p->buffer() < desc->buf_end
            && noutputs == 0) {
            // put the original buffer in the freelist
            NetmapInfo::buffer_destructor(buf, 0, (void *)this);
            // now enqueue
            ring->slot[cur].buf_idx = NETMAP_BUF_IDX(ring, (char *) p->buffer());
            ring->slot[cur].flags |= NS_BUF_CHANGED;
            // and make sure nobody uses this packet
            p->reset_buffer();
        } else
            memcpy(buf, p->data(), p_length);
        ring->slot[cur].len = p_length;
        __asm__ volatile("" : : : "memory");
        ring->head = ring->cur = nm_ring_next(ring, cur);
        return 0;
    }
    errno = ENOBUFS;
    return -1;
#endif
}
void
NetmapInfo::close(int fd)
{
    click_chatter("fd %d interface %s\n",
	fd, desc->req.nr_name);
    netmap_memory_lock.acquire();
    // unlink from the list ?
    nm_close(desc);
    desc = 0;
    netmap_memory_lock.release();
}

CLICK_ENDDECLS
#endif
ELEMENT_PROVIDES(NetmapInfo)
