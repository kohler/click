/*
 * todpdkdevice.{cc,hh} -- element sends packets to network via Intel's DPDK
 *
 * Copyright (c) 2014-2015 Cyril Soldani, University of Liège
 * Copyright (c) 2015 Tom Barbette, University of Liège
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

#include <click/args.hh>
#include <click/error.hh>
#include <click/algorithm.hh>

#include "todpdkdevice.hh"

CLICK_DECLS

ToDPDKDevice::ToDPDKDevice() :
    _iqueues(), _dev(0), _queue_id(0), _blocking(false),
    _iqueue_size(1024), _burst_size(32), _timeout(0), _n_sent(0),
    _n_dropped(0), _congestion_warning_printed(false)
{
}

ToDPDKDevice::~ToDPDKDevice()
{
}

int ToDPDKDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int n_desc = -1;
	String dev;
	bool allow_nonexistent = false;

    if (Args(conf, this, errh)
        .read_mp("PORT", dev)
        .read_p("QUEUE", _queue_id)
        .read("IQUEUE", _iqueue_size)
        .read("BLOCKING", _blocking)
        .read("BURST", _burst_size)
        .read("TIMEOUT", _timeout)
        .read("NDESC",n_desc)
        .read("ALLOW_NONEXISTENT", allow_nonexistent)
        .complete() < 0)
        return -1;

    if (_iqueue_size < _burst_size) {
        _iqueue_size = _burst_size;
        click_chatter(
            "%s: IQUEUE cannot be smaller than BURST, IQUEUE has been set to "
            "match BURST, that is %d", name().c_str(), _iqueue_size);
    }

    if (!DPDKDeviceArg::parse(dev, _dev)) {
        if (allow_nonexistent)
            return 0;
        else
            return errh->error("%s : Unknown or invalid PORT", dev.c_str());
    }

    return _dev->add_tx_queue(_queue_id, (n_desc > 0) ? n_desc : 1024, errh);
}

int ToDPDKDevice::initialize(ErrorHandler *errh)
{
    if (!_dev)
        return 0;

    _iqueues.resize(click_max_cpu_ids());

    for (int i = 0; i < _iqueues.size(); i++) {
        _iqueues[i].pkts = new struct rte_mbuf *[_iqueue_size];
        if (_timeout >= 0) {
            _iqueues[i].timeout.assign(this);
            _iqueues[i].timeout.initialize(this);
        }
    }

    return DPDKDevice::initialize(errh);
}

void ToDPDKDevice::cleanup(CleanupStage)
{
    for (int i = 0; i < _iqueues.size(); i++)
        delete[] _iqueues[i].pkts;
}

String ToDPDKDevice::n_sent_handler(Element *e, void *)
{
    ToDPDKDevice *tdd = static_cast<ToDPDKDevice *>(e);
    return String(tdd->_n_sent);
}

String ToDPDKDevice::n_dropped_handler(Element *e, void *)
{
    ToDPDKDevice *tdd = static_cast<ToDPDKDevice *>(e);
    return String(tdd->_n_dropped);
}

int ToDPDKDevice::reset_counts_handler(const String &, Element *e, void *,
                                       ErrorHandler *)
{
    ToDPDKDevice *tdd = static_cast<ToDPDKDevice *>(e);
    tdd->_n_sent = 0;
    tdd->_n_dropped = 0;
    return 0;
}

void ToDPDKDevice::add_handlers()
{
    add_read_handler("n_sent", n_sent_handler, 0);
    add_read_handler("n_dropped", n_dropped_handler, 0);
    add_write_handler("reset_counts", reset_counts_handler, 0,
                      Handler::BUTTON);
}

/* Return the rte_mbuf pointer for a packet. If the buffer of the packet is
 * from a DPDK pool, it will return the underlying rte_mbuf and remove the
 * destructor. If it's a Click buffer, it will allocate a DPDK mbuf and copy
 * the packet content to it if create is true. */
inline struct rte_mbuf* get_mbuf(Packet* p, bool create=true) {
    struct rte_mbuf* mbuf = 0;

    if (likely(DPDKDevice::is_dpdk_packet(p))) {
        mbuf = (struct rte_mbuf *) p->destructor_argument();
        rte_pktmbuf_pkt_len(mbuf) = p->length();
        rte_pktmbuf_data_len(mbuf) = p->length();
        mbuf->data_off = p->headroom();
        if (p->shared()) {
            /*Prevent DPDK from freeing the buffer. When all shared packet
             * are freed, DPDKDevice::free_pkt will effectively destroy it.*/
            rte_mbuf_refcnt_update(mbuf, 1);
        } else {
            //Reset buffer, let DPDK free the buffer when it wants
            p->reset_buffer();
        }
    } else if (create) {
        mbuf = rte_pktmbuf_alloc(DPDKDevice::get_mpool(rte_socket_id()));
        memcpy((void*) rte_pktmbuf_mtod(mbuf, unsigned char *), p->data(),
               p->length());
        rte_pktmbuf_pkt_len(mbuf) = p->length();
        rte_pktmbuf_data_len(mbuf) = p->length();
    }

    return mbuf;
}

void ToDPDKDevice::run_timer(Timer *)
{
    flush_internal_queue(_iqueues[click_current_cpu_id()]);
}

/* Flush as much as possible packets from a given internal queue to the DPDK
 * device. */
void ToDPDKDevice::flush_internal_queue(InternalQueue &iqueue) {
    unsigned sent = 0;
    unsigned r;
    /* sub_burst is the number of packets DPDK should send in one call if
     * there is no congestion, normally 32. If it sends less, it means
     * there is no more room in the output ring and we'll need to come
     * back later. Also, if we're wrapping around the ring, sub_burst
     * will be used to split the burst in two, as rte_eth_tx_burst needs a
     * contiguous buffer space.
     */
    unsigned sub_burst;

    _lock.acquire();

    do {
        sub_burst = iqueue.nr_pending > 32 ? 32 : iqueue.nr_pending;
        if (iqueue.index + sub_burst >= _iqueue_size)
            // The sub_burst wraps around the ring
            sub_burst = _iqueue_size - iqueue.index;
        r = rte_eth_tx_burst(_dev->port_id, _queue_id, &iqueue.pkts[iqueue.index],
                             sub_burst);

        iqueue.nr_pending -= r;
        iqueue.index += r;

        if (iqueue.index >= _iqueue_size) // Wrapping around the ring
            iqueue.index = 0;

        sent += r;
    } while (r == sub_burst && iqueue.nr_pending > 0);

    _n_sent += sent;

    _lock.release();

    // If ring is empty, reset the index to avoid wrap ups
    if (iqueue.nr_pending == 0)
        iqueue.index = 0;
}

void ToDPDKDevice::push(int, Packet *p)
{
    if (!_dev)
        return;

    // Get the thread-local internal queue
    InternalQueue &iqueue = _iqueues[click_current_cpu_id()];

    bool congestioned;
    do {
        congestioned = false;

        if (iqueue.nr_pending == _iqueue_size) { // Internal queue is full
            /* We just set the congestion flag. If we're in blocking mode,
             * we'll loop, else we'll drop this packet.*/
            congestioned = true;
            if (!_blocking) {
                if (_n_dropped < 5)
                    click_chatter("%s: packet dropped", name().c_str());
                _n_dropped++;
            } else {
                if (!_congestion_warning_printed)
                    click_chatter("%s: congestion warning", name().c_str());
                _congestion_warning_printed = true;
            }
        } else { // If there is space in the iqueue just after index + left
            iqueue.pkts[(iqueue.index + iqueue.nr_pending) % _iqueue_size] =
                get_mbuf(p);
            iqueue.nr_pending++;
        }

        if (iqueue.nr_pending >= _burst_size || congestioned) {
            flush_internal_queue(iqueue);
            if (_timeout && iqueue.nr_pending == 0)
                iqueue.timeout.unschedule();
        } else if (_timeout >= 0 && !iqueue.timeout.scheduled()) {
            if (_timeout == 0)
                iqueue.timeout.schedule_now();
            else
                iqueue.timeout.schedule_after_msec(_timeout);
        }

        // If we're in blocking mode, we loop until we can put p in the iqueue
    } while (unlikely(_blocking && congestioned));

    p->kill();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel dpdk)
EXPORT_ELEMENT(ToDPDKDevice)
ELEMENT_MT_SAFE(ToDPDKDevice)
