// -*- c-basic-offset: 4 -*-
/*
 * ipreassembler.{cc,hh} -- defragments IP packets
 * Alexander Yip, Eddie Kohler
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
#include <click/config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "ipreassembler.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#define PACKET_LINK(p)		((PacketLink *)((p)->all_user_anno_u()))
#define PACKET_DLEN(p)		((p)->transport_length())
#define Q_PACKET_JIFFIES(p)	((p)->timestamp_anno().tv_usec)
#define IP_BYTE_OFF(iph)	((ntohs((iph)->ip_off) & IP_OFFMASK) << 3)

IPReassembler::IPReassembler()
    : Element(1, 1), _expire_timer(expire_hook, (void *) this)
{
    MOD_INC_USE_COUNT;
    for (int i = 0; i < NMAP; i++)
	_map[i] = 0;
    static_assert(sizeof(PacketLink) <= Packet::USER_ANNO_SIZE);
    static_assert(sizeof(ChunkLink) <= 8);
}

IPReassembler::~IPReassembler()
{
    MOD_DEC_USE_COUNT;
}

int
IPReassembler::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _mem_high_thresh = 256 * 1024;
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "HIMEM", cpUnsigned, "memory consumption limit", &_mem_high_thresh,
		    0) < 0)
	return -1;
    _mem_low_thresh = (_mem_high_thresh >> 2) * 3;
    return 0;
}

int
IPReassembler::initialize(ErrorHandler *)
{
    _expire_timer.initialize(this);
    _expire_timer.schedule_after_ms(EXPIRE_TIMER_INTERVAL_MS);
    _mem_used = 0;
    return 0;
}

void
IPReassembler::cleanup(CleanupStage)
{
    for (int i = 0; i < NMAP; i++)
	while (_map[i]) {
	    WritablePacket *next = PACKET_LINK(_map[i])->bucket_next;
	    _map[i]->kill();
	    _map[i] = next;
	}
}

void
IPReassembler::check_error(ErrorHandler *errh, int bucket, const Packet *p, const char *format, ...)
{
    const click_ip *iph = p->ip_header();
    va_list val;
    va_start(val, format);
    StringAccum sa;
    sa << "buck " << bucket << ": ";
    if (iph)
	sa << iph->ip_src << " > " << iph->ip_dst << " [" << ntohs(iph->ip_id) << ':' << PACKET_DLEN(p) << ((iph->ip_off & htons(IP_MF)) ? "+]: " : "]: ");
    sa << format;
    errh->verror(ErrorHandler::ERR_ERROR, String(), sa.cc(), val);
    va_end(val);
}

int
IPReassembler::check(ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    uint32_t mem_used = 0;
    for (int b = 0; b < NMAP; b++)
	for (WritablePacket *q = _map[b]; q; q = PACKET_LINK(q)->bucket_next)
	    if (const click_ip *qip = q->ip_header()) {
		if (bucketno(qip) != b)
		    check_error(errh, b, q, "in wrong bucket");
		mem_used += IPH_MEM_USED + q->transport_length();
		ChunkLink *chunk = &PACKET_LINK(q)->chunk;
		int off = 0;
#if VERBOSE_DEBUG
		check_error(errh, b, q, "");
		StringAccum sa;
		while (chunk && (!off || off < q->transport_length())) {
		    sa << " (" << chunk->off << ',' << chunk->lastoff << ')';
		    off = chunk->lastoff;
		    chunk = next_chunk(q, chunk);
		}
		errh->message("  %s", sa.cc());
		chunk = &PACKET_LINK(q)->chunk;
		off = 0;
#endif
		while (chunk) {
		    if (chunk->off >= chunk->lastoff
			|| chunk->lastoff > q->transport_length()
			|| (off != 0 && chunk->off < off + 8)) {
			check_error(errh, b, q, "bad chunk (%d, %d) at %d", chunk->off, chunk->lastoff, off);
			break;
		    }
		    off = chunk->lastoff;
		    chunk = next_chunk(q, chunk);
		}
	    } else
		errh->error("buck %d: missing IP header", b);
    if (mem_used != _mem_used)
	errh->error("bad mem_used: have %u, claim %u", mem_used, _mem_used);
    return 0;
}

WritablePacket *
IPReassembler::find_queue(Packet *p, WritablePacket ***store_pprev)
{
    const click_ip *iph = p->ip_header();
    int bucket = bucketno(iph);
    WritablePacket **pprev = &_map[bucket];
    WritablePacket *q;
    for (q = *pprev; q; pprev = &PACKET_LINK(q)->bucket_next, q = *pprev) {
	const click_ip *qiph = q->ip_header();
	if (same_segment(iph, qiph)) {
	    *store_pprev = pprev;
	    return q;
	}
    }
    *store_pprev = &_map[bucket];
    return 0;
}

Packet *
IPReassembler::emit_whole_packet(WritablePacket *q, WritablePacket **q_pprev,
				 Packet *p_in)
{
    click_ip *q_iph = q->ip_header();
    q_iph->ip_len = htons(q->network_length());
    q_iph->ip_sum = 0;
    q_iph->ip_sum = click_in_cksum((const unsigned char *)q_iph, q_iph->ip_hl << 2);

    // zero out the annotations we used
    memset(&PACKET_LINK(q)->bucket_next, 0, sizeof(struct PacketLink) - offsetof(struct PacketLink, bucket_next));
    q->set_timestamp_anno(p_in->timestamp_anno());

    *q_pprev = PACKET_LINK(q)->bucket_next;

    p_in->kill();
    _mem_used -= IPH_MEM_USED + q->transport_length();
    return q;
}

void
IPReassembler::make_queue(Packet *p, WritablePacket **q_pprev)
{
    const click_ip *iph = p->ip_header();
    int p_off = IP_BYTE_OFF(iph);
    int p_lastoff = p_off + PACKET_DLEN(p);

    int hl = (p_off == 0 ? iph->ip_hl << 2 : 20);
    WritablePacket *q = Packet::make(60 - hl, 0, hl + p_lastoff, 0);
    if (!q) {
	click_chatter("out of memory");
	return;
    }
    _mem_used += IPH_MEM_USED + p_lastoff;

    // copy IP header and annotations if appropriate
    q->set_ip_header((click_ip *)q->data(), hl);
    memcpy(q->ip_header(), iph, hl);
    click_ip *q_iph = q->ip_header();
    q_iph->ip_off = (iph->ip_off & ~htons(IP_OFFMASK)); // leave MF, DF, RF
    if (p_off == 0)
	q->copy_annotations(p);
    
    // copy data
    memcpy(q->transport_header() + p_off, p->transport_header(), PACKET_DLEN(p));
    PACKET_LINK(q)->chunk.off = p_off;
    PACKET_LINK(q)->chunk.lastoff = p_lastoff;

    // link it up
    PACKET_LINK(q)->bucket_next = *q_pprev;
    *q_pprev = q;
    Q_PACKET_JIFFIES(q) = click_jiffies();
    
    check();
}

IPReassembler::ChunkLink *
IPReassembler::next_chunk(WritablePacket *q, ChunkLink *chunk)
{
    if (chunk->lastoff >= q->transport_length())
	return 0;
    else
	return (ChunkLink *)(q->transport_header() + chunk->lastoff);
}

Packet *
IPReassembler::simple_action(Packet *p)
{
    // check common case: not a fragment 
    const click_ip *iph = p->ip_header();
    assert(iph);
    if (!IP_ISFRAG(iph))
	return p;

    // calculate packet edges
    int p_off = IP_BYTE_OFF(iph);
    int p_lastoff = p_off + ntohs(iph->ip_len) - (iph->ip_hl << 2);

    // check uncommon, but annoying, case: bad length, bad length + offset,
    // or middle fragment length not a multiple of 8 bytes
    if (p_lastoff > 0xFFFF || p_lastoff <= p_off
	|| ((p_lastoff & 7) != 0 && (iph->ip_off & htons(IP_MF)) != 0)
	|| PACKET_DLEN(p) < p_lastoff - p_off) {
	p->kill();
	return 0;
    }
    p->take(PACKET_DLEN(p) - (p_lastoff - p_off));

    // otherwise, we need to keep the packet

    // clean up memory if necessary
    if (_mem_used > _mem_high_thresh)
	garbage_collect();

    // get its Packet queue
    WritablePacket **q_pprev;
    WritablePacket *q = find_queue(p, &q_pprev);
    if (!q) {			// make a new queue
	make_queue(p, q_pprev);
	return 0;
    }
    WritablePacket *q_bucket_next = PACKET_LINK(q)->bucket_next;

    // extend the packet if necessary
    if (p_lastoff > q->transport_length()) {
	// error if packet already completed
	if (!(q->ip_header()->ip_off & htons(IP_MF))) {
	    p->kill();
	    return 0;
	}
	// Figure out how much space to request. Add 8 extra bytes to ensure
	// room for a ChunkLink, and request extra space if this packet has MF
	// set. XXX This algorithm could result in a number of intermediate
	// packet copies linear in the final packet length.
	int old_transport_length = q->transport_length();
	assert((old_transport_length & 7) == 0);
	int want_space = p_lastoff - old_transport_length + 8;
	if (iph->ip_off & htons(IP_MF))
	    want_space += (p_lastoff - p_off);
	// request space
	if (!(q = q->put(want_space))) {
	    click_chatter("out of memory");
	    *q_pprev = q_bucket_next;
	    p->kill();
	    _mem_used -= IPH_MEM_USED + old_transport_length;
	    return 0;
	}
	// get rid of extra space
	q->take(q->transport_length() - p_lastoff);
	// hook up packet, and add final chunk
	*q_pprev = q;
	ChunkLink *last_chunk = (ChunkLink *)(q->transport_header() + old_transport_length);
	last_chunk->off = last_chunk->lastoff = p_lastoff;
	_mem_used += p_lastoff - old_transport_length;
    }

    // find chunks before and after p
    ChunkLink *chunk = &PACKET_LINK(q)->chunk;
    while (chunk->lastoff < p_off)
	chunk = next_chunk(q, chunk);
    ChunkLink *last = chunk;
    while (last && last->lastoff < p_lastoff)
	last = next_chunk(q, last);

    // patch chunks
    assert(chunk && last);
    if (p_lastoff < last->off) {
	ChunkLink *new_chunk = (ChunkLink *)(q->transport_header() + p_lastoff);
	*new_chunk = *last;
	chunk->lastoff = p_lastoff;
    } else
	chunk->lastoff = last->lastoff;
    if (p_off < chunk->off)
	chunk->off = p_off;

    // copy p's data into q
    memcpy(q->transport_header() + p_off, p->transport_header(), p_lastoff - p_off);

    // copy p's annotations and IP header if it is the first packet
    if (p_off == 0) {
	int old_ip_off = q->ip_header()->ip_off;
	int hl = iph->ip_hl << 2;
	if (hl > (int) q->network_header_length())
	    (void) q->nonunique_push(hl - q->network_header_length());
	else
	    q->pull(q->network_header_length() - hl);
	q->set_ip_header((click_ip *)(q->transport_header() - hl), hl);
	memcpy(q->ip_header(), p->ip_header(), hl);
	q->ip_header()->ip_off = old_ip_off;
	//q->copy_annotations(p); XXX
    }

    // clear MF if incoming packet has it cleared
    if (!(iph->ip_off & htons(IP_MF)))
	q->ip_header()->ip_off &= ~htons(IP_MF);
    
    // Are we done with this packet?
    if ((q->ip_header()->ip_off & htons(IP_MF)) == 0
	&& PACKET_LINK(q)->chunk.off == 0
	&& PACKET_LINK(q)->chunk.lastoff == q->transport_length())
	return emit_whole_packet(q, q_pprev, p);

    // Otherwise, done for now
    // mark the queue packet with the current time
    Q_PACKET_JIFFIES(q) = click_jiffies();
    //check();
    return 0;
}

void
IPReassembler::garbage_collect()
{
    check();

    int now = click_jiffies();

    // First throw away fragments at least 10 seconds old, then at least 5
    // seconds old, then any fragments.
    for (int delta = 10; delta >= 0; delta -= 5)
	for (int bucket = 0; bucket < NMAP; bucket++) {
	    WritablePacket **pprev = &_map[bucket];
	    for (WritablePacket *q = *pprev; q; q = *pprev)
		if (Q_PACKET_JIFFIES(q) <= now - delta) {
		    *pprev = PACKET_LINK(q)->bucket_next;
		    _mem_used -= IPH_MEM_USED + q->transport_length();
		    q->kill();
		    if (_mem_used <= _mem_low_thresh)
			return;
		} else
		    pprev = &PACKET_LINK(q)->bucket_next;
	}

    click_chatter("IPReassembler: cannot free enough memory!");
}

void
IPReassembler::expire_hook(Timer *, void *thunk)
{
    // look at all queues. If no activity for 30 seconds, kill that queue

    IPReassembler *ipr = reinterpret_cast<IPReassembler *>(thunk);
    int kill_time = click_jiffies() - EXPIRE_TIMEOUT * CLICK_HZ;

    for (int i = 0; i < NMAP; i++) {
	WritablePacket **q_pprev = &ipr->_map[i];
	for (WritablePacket *q = *q_pprev; q; ) {
	    if (Q_PACKET_JIFFIES(q) < kill_time) {
		*q_pprev = PACKET_LINK(q)->bucket_next;
		q->kill();
	    } else
		q_pprev = &PACKET_LINK(q)->bucket_next;
	    q = *q_pprev;
	}
    }

    ipr->_expire_timer.schedule_after_ms(EXPIRE_TIMER_INTERVAL_MS);
}

EXPORT_ELEMENT(IPReassembler)
