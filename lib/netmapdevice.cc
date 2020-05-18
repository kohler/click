// -*- c-basic-offset: 4; related-file-name: "../include/click/netmapdevice.hh" -*-
/*
 * netmapinfo.{cc,hh} -- library for interfacing with netmap
 * Eddie Kohler, Luigi Rizzo, Tom Barbette
 *
 * Copyright (c) 2012 Eddie Kohler
 * Copyright (c) 2015 University of Liege
 *
 * NetmapBufQ implementation was started by Luigi Rizzo and moved from netmapinfo.hh.
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
#include <click/netmapdevice.hh>
#include <sys/mman.h>
#include <sys/ioctl.h>

/****************************
 * NetmapBufQ
 ****************************/
NetmapBufQ::NetmapBufQ() : _head(0),_count(0){

}

NetmapBufQ::~NetmapBufQ() {

}

/**
 * Initizlize the per-thread pools and the cleanup pool
 */
int NetmapBufQ::static_initialize(struct nm_desc* nmd) {
    if (!nmd) {
        click_chatter("Error:Null netmap descriptor in NetmapBufQ::static_initialize!");
        return 1;
    }

    //Only initilize once
    if (buf_size)
        return 0;

    buf_size = nmd->some_ring->nr_buf_size;
    buf_start = reinterpret_cast<unsigned char *>(nmd->buf_start);
    buf_end = reinterpret_cast<unsigned char *>(nmd->buf_end);
    max_index = (buf_end - buf_start) / buf_size;

    if (!netmap_buf_pools) {
        netmap_buf_pools = new NetmapBufQ*[click_max_cpu_ids()];

        for (unsigned i = 0; i < click_max_cpu_ids(); i++) {
            netmap_buf_pools[i] = new NetmapBufQ();
        }
    }

    return 0;
}

/**
 * Empty all NetmapBufQ and the global ring. Return all netmap buffers in a list
 */
uint32_t NetmapBufQ::static_cleanup()
{
    if (!netmap_buf_pools)
        return 0;

    for (unsigned int i = 1; i < click_max_cpu_ids(); i++) {
        if (netmap_buf_pools[i]) {
            if (netmap_buf_pools[i]->_head)
                netmap_buf_pools[0]->insert_all(netmap_buf_pools[i]->_head, false);
            delete netmap_buf_pools[i];
            netmap_buf_pools[i] = NULL;
        }
    }

    while (global_buffer_list > 0) {
        uint32_t idx=global_buffer_list;
        global_buffer_list = BUFFER_NEXT_LIST(global_buffer_list);
        netmap_buf_pools[0]->insert_all(idx, false);
    }

    uint32_t idx = 0;
    if (netmap_buf_pools[0]->_count > 0) {
        if (netmap_buf_pools[0]->_count == netmap_buf_pools[0]->count_buffers(idx))
            click_chatter("Error on cleanup of netmap buffer ! Expected %d buffer, got %d",netmap_buf_pools[0]->_count,netmap_buf_pools[0]->count_buffers(idx));
        else
            click_chatter("Freeing %d Netmap buffers",netmap_buf_pools[0]->_count);
        idx = netmap_buf_pools[0]->_head;
        netmap_buf_pools[0]->_head = 0;
        netmap_buf_pools[0]->_count = 0;
    }
    delete netmap_buf_pools[0];
    netmap_buf_pools[0] = 0;
    delete[] netmap_buf_pools;
    return idx;
}

/**
 * Insert all netmap buffers inside the global list
 */
void NetmapBufQ::global_insert_all(uint32_t idx, int count) {
    //Cut packets in global pools
    while (count >= NETMAP_PACKET_POOL_SIZE) {
        int c = 0;
        BUFFER_NEXT_LIST(idx) = global_buffer_list;
        global_buffer_list = idx;
        uint32_t *p = 0;
        while (c < NETMAP_PACKET_POOL_SIZE) {
            p = reinterpret_cast<uint32_t*>((unsigned char *)buf_start +
                    idx * buf_size);
            idx = *p;
            c++;
        }
        *p = 0;
        count -= NETMAP_PACKET_POOL_SIZE;
    }

    //Add remaining buffer to the local pool
    if (count > 0) {
        NetmapBufQ::local_pool()->insert_all(idx, true);
    }
}

/***************************
 * NetmapDevice
 ***************************/

/*
 * keep a list of netmap ports so matching the name we
 * can recycle the regions
 */
static Spinlock netmap_memory_lock;

int
NetmapDevice::open(const String &ifname,
               bool always_error, ErrorHandler *errh)
{
    click_chatter("%s ifname %s\n", __FUNCTION__, ifname.c_str());
    ErrorHandler *initial_errh = always_error ? errh : ErrorHandler::silent_handler();

    netmap_memory_lock.acquire();
    do {
    struct nm_desc* base_nmd = (struct nm_desc*)calloc(1,sizeof(struct nm_desc));

    base_nmd->self = base_nmd;
    strcpy(base_nmd->req.nr_name,&(ifname.c_str()[7]));
    if (NetmapDevice::some_nmd != NULL) { //Having same netmap space is a lot easier...
        base_nmd->mem = NetmapDevice::some_nmd->mem;
        base_nmd->memsize = NetmapDevice::some_nmd->memsize;
        base_nmd->req.nr_arg2 = NetmapDevice::some_nmd->req.nr_arg2;
        base_nmd->req.nr_arg3 = 0;
        base_nmd->done_mmap = NetmapDevice::some_nmd->done_mmap;
        desc = nm_open(ifname.c_str(), NULL, NM_OPEN_NO_MMAP | NM_OPEN_IFNAME, base_nmd);
    } else {
        base_nmd->req.nr_arg3 = NetmapDevice::global_alloc;
        if (base_nmd->req.nr_arg3 % NETMAP_PACKET_POOL_SIZE != 0)
            base_nmd->req.nr_arg3 = ((base_nmd->req.nr_arg3 / NETMAP_PACKET_POOL_SIZE) + 1) * NETMAP_PACKET_POOL_SIZE;
        //Ensure we have at least a batch per thread + 1
        if (NETMAP_PACKET_POOL_SIZE * ((unsigned)click_nthreads + 1) > base_nmd->req.nr_arg3)
            base_nmd->req.nr_arg3 = NETMAP_PACKET_POOL_SIZE * (click_nthreads + 1);
        desc = nm_open(ifname.c_str(), NULL, NM_OPEN_IFNAME | NM_OPEN_ARG3, base_nmd);
        NetmapDevice::some_nmd = desc;
    }

    if (desc == NULL) {
        initial_errh->error("nm_open(%s): %s", ifname.c_str(), strerror(errno));
        break;
    }
    click_chatter("%s %s memsize %d mem %p buf_start %p buf_end %p",
        __FUNCTION__, desc->req.nr_name,
        desc->memsize, desc->mem, desc->buf_start, desc->buf_end);

    /* eventually try to match the region */
    click_chatter("private mapping for %s\n", ifname.c_str());
    } while (0);

    //Allocate packet pools if not already done
    NetmapBufQ::static_initialize(desc);

    if (desc->req.nr_arg3 > 0) {
        click_chatter("Allocated %d buffers from Netmap buffer pool",desc->req.nr_arg3);
        NetmapBufQ::global_insert_all(desc->nifp->ni_bufs_head,desc->req.nr_arg3);
        desc->nifp->ni_bufs_head = 0;
        desc->req.nr_arg3 = 0;
    }

    netmap_memory_lock.release();
    return desc ? desc->fd : -1;
}

void
NetmapDevice::initialize_rings_rx(int timestamp)
{
    click_chatter("%s timestamp %d\n", __FUNCTION__, timestamp);
    if (timestamp >= 0) {
    int flags = (timestamp > 0 ? NR_TIMESTAMP : 0);
    for (unsigned i = desc->first_rx_ring; i <= desc->last_rx_ring; ++i)
        NETMAP_RXRING(desc->nifp, i)->flags = flags;
    }
}

void
NetmapDevice::initialize_rings_tx()
{
    click_chatter("%s\n", __FUNCTION__);
}

int
NetmapDevice::receive(int cnt, int headroom, void (*emit_packet)(WritablePacket *, int, const Timestamp &, void* arg), void* arg)
{
    int n = desc->last_rx_ring - desc->first_rx_ring + 1;
    int c, got = 0, ri = desc->cur_rx_ring;

    if (cnt == 0)
        cnt = -1;
    /* cnt == -1 means infinite, but rings have a finite amount
     * of buffers and the int is large enough that we never wrap,
     * so we can omit checking for -1
     */
    for (c=0; c < n && cnt != got; c++) {
        /* compute current ring to use */
        struct netmap_ring *ring;

        ri = desc->cur_rx_ring + c;
        if (ri > desc->last_rx_ring)
            ri = desc->first_rx_ring;
        ring = NETMAP_RXRING(desc->nifp, ri);
        for ( ; !nm_ring_empty(ring) && cnt != got; got++) {
            u_int i = ring->cur;
            u_int idx = ring->slot[i].buf_idx;
            u_int new_buf = NetmapBufQ::local_pool()->extract();
            u_char *buf = (u_char *)NETMAP_BUF(ring, idx);
            u_int len = ring->slot[i].len;
            WritablePacket *p;

            if (new_buf) {
                ring->slot[i].buf_idx = new_buf;
                ring->slot[i].flags |= NS_BUF_CHANGED;
                p = Packet::make(buf,len,NetmapBufQ::buffer_destructor,0);
                __builtin_prefetch(buf);
            } else {
                p = Packet::make(headroom, buf, len, 0);
            }
            Timestamp ts = Timestamp::uninitialized_t();
        #if TIMESTAMP_NANOSEC && defined(PCAP_TSTAMP_PRECISION_NANO)
            if (_pcap_nanosec)
                ts = Timestamp::make_nsec(ring->ts.tv_sec, ring->ts.tv_usec);
            else
        #endif
                ts = Timestamp::make_usec(ring->ts.tv_sec, ring->ts.tv_usec);

            emit_packet(p, len, ts, arg);

            ring->head = ring->cur = nm_ring_next(ring, i);
        }
    }
    desc->cur_rx_ring = ri;
    return got;
}



int NetmapDevice::send_packet(Packet* p,bool allow_zc) {
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
        if (NetmapBufQ::is_valid_netmap_packet(p)
            && !p->shared()
            && allow_zc) {
            //Get the buffer currently in the ring and put it in the buffer queue
            NetmapBufQ::local_pool()->insert(ring->slot[cur].buf_idx);

            //Replace the ring buffer by the one from the packet
            ring->slot[cur].buf_idx = NETMAP_BUF_IDX(ring, (char *) p->buffer());
            ring->slot[cur].flags |= NS_BUF_CHANGED;
            if (cur % 32 == 0)
                ring->slot[cur].flags |= NS_REPORT;

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
}



void
NetmapDevice::close(int fd)
{
    click_chatter("fd %d interface %s\n",
    fd, desc->req.nr_name);
    if (desc != NetmapDevice::some_nmd) {
        netmap_memory_lock.acquire();
        // unlink from the list ?
        nm_close(desc);
        desc = 0;
        netmap_memory_lock.release();
    }
}

void NetmapDevice::static_cleanup() {
    uint32_t idx = NetmapBufQ::static_cleanup();
    if (idx != 0) {
        if (some_nmd) {
            some_nmd->nifp->ni_bufs_head = idx;
            nm_close(some_nmd);
            some_nmd = 0;
        } else {
            click_chatter("No NMD set and netmap packet not released !");
        }
    }
}

NetmapBufQ** NetmapBufQ::netmap_buf_pools = 0;
unsigned int NetmapBufQ::buf_size = 0;
unsigned char* NetmapBufQ::buf_start = 0;
unsigned char* NetmapBufQ::buf_end = 0;
uint32_t NetmapBufQ::max_index = 0;

Spinlock NetmapBufQ::global_buffer_lock;
uint32_t NetmapBufQ::global_buffer_list = 0;

int NetmapBufQ::messagelimit = 0;

int NetmapDevice::global_alloc = 32768;
struct nm_desc* NetmapDevice::some_nmd = 0;

CLICK_ENDDECLS
