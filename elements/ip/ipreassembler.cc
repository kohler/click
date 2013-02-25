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

#include <click/config.h>
#include "ipreassembler.hh"
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <click/ipflowid.hh>
CLICK_DECLS

#define PACKET_CHUNK(p)		(*((ChunkLink *)((p)->anno_u8() + IPREASSEMBLER_ANNO_OFFSET)))
#define PACKET_DLEN(p)		((p)->transport_length())
#define IP_BYTE_OFF(iph)	((ntohs((iph)->ip_off) & IP_OFFMASK) << 3)

IPReassembler::IPReassembler()
    : _stat_frags_seen(0), _stat_good_assem(0), _stat_failed_assem(0), _stat_bad_pkts(0)
{
    for (int i = 0; i < NMAP; i++)
	_map[i] = 0;
    static_assert(IPREASSEMBLER_ANNO_OFFSET + IPREASSEMBLER_ANNO_SIZE <= Packet::anno_size, "anno too big");
    static_assert(sizeof(ChunkLink) == IPREASSEMBLER_ANNO_SIZE, "sizeof(ChunkLink) is expected to equal IPREASSEMBLER_ANNO_SIZE.");
}

IPReassembler::~IPReassembler()
{
}

int
IPReassembler::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _mem_high_thresh = 256 * 1024;
    int mtu_anno = -1;
    if (Args(conf, this, errh)
	.read("HIMEM", _mem_high_thresh)
	.read("MAX_MTU_ANNO", AnnoArg(2), mtu_anno)
	.complete() < 0)
	return -1;
    _mtu_anno = mtu_anno;
    _mem_low_thresh = (_mem_high_thresh >> 2) * 3;
    return 0;
}

int
IPReassembler::initialize(ErrorHandler *)
{
    _mem_used = 0;
    _reap_time = 0;
    return 0;
}

void
IPReassembler::cleanup(CleanupStage)
{
    for (int i = 0; i < NMAP; i++)
	while (_map[i]) {
	    WritablePacket *next = (WritablePacket *)(_map[i]->next());
	    _map[i]->kill();
	    _map[i] = next;
	}
}

void
IPReassembler::check_error(ErrorHandler *errh, int bucket, const Packet *p, const char *format, ...)
{
    va_list val;
    va_start(val, format);
    StringAccum sa;
    sa << "buck " << bucket << ": ";
    if (p->has_network_header()) {
	const click_ip *iph = p->ip_header();
	sa << iph->ip_src << " > " << iph->ip_dst << " [" << ntohs(iph->ip_id) << ':' << PACKET_DLEN(p) << ((iph->ip_off & htons(IP_MF)) ? "+]: " : "]: ");
    }
    sa << format;
    errh->xmessage(ErrorHandler::e_error, sa.c_str(), val);
    va_end(val);
}

int
IPReassembler::check(ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    uint32_t mem_used = 0;
    for (int b = 0; b < NMAP; b++)
	for (WritablePacket *q = _map[b]; q; q = (WritablePacket *)(q->next()))
	    if (q->has_network_header()) {
		const click_ip *qip = q->ip_header();
		if (bucketno(qip) != b)
		    check_error(errh, b, q, "in wrong bucket");
		mem_used += IPH_MEM_USED + q->transport_length();
		ChunkLink *chunk = &PACKET_CHUNK(q);
		int off = 0;
#if VERBOSE_DEBUG
		check_error(errh, b, q, "");
		StringAccum sa;
		while (chunk && (!off || off < q->transport_length())) {
		    sa << " (" << chunk->off << ',' << chunk->lastoff << ')';
		    off = chunk->lastoff;
		    chunk = next_chunk(q, chunk);
		}
		errh->message("  %s", sa.c_str());
		chunk = &PACKET_CHUNK(q);
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

String
IPReassembler::debug_dump(Element *e, void *)
{
    IPReassembler *r = (IPReassembler *) e;
    r->check();
    StringAccum sa;
    sa <<
	"frags seen total:    " << r->_stat_frags_seen << "\n"
	"good reassemblies:   " << r->_stat_good_assem << "\n"
	"failed reassemblies: " << r->_stat_failed_assem << "\n"
	"bad fragments seen:  " << r->_stat_bad_pkts << "\n"
	"cached chunk data:\n";
    for (int b = 0; b < NMAP; b++)
	for (WritablePacket *q = r->_map[b]; q; q = (WritablePacket *)(q->next()))
	    if (const click_ip *qip = q->ip_header()) {
		sa << ' ' << IPFlowID(qip) << ' ' << ntohs(qip->ip_id);
		ChunkLink *chunk = &PACKET_CHUNK(q);
		while (chunk &&
		       (chunk->lastoff > chunk->off) &&
		       (chunk->lastoff <= q->transport_length())) {
		    sa << " (" << chunk->off << ',' << chunk->lastoff << ')';
		    chunk = next_chunk(q, chunk);
		}
		sa << '\n';
	    }
    return sa.take_string();
}

WritablePacket *
IPReassembler::find_queue(Packet *p, WritablePacket ***store_pprev)
{
    const click_ip *iph = p->ip_header();
    int bucket = bucketno(iph);
    WritablePacket **pprev = &_map[bucket];
    WritablePacket *q;
    for (q = *pprev; q; pprev = (WritablePacket **)&q->next(), q = *pprev) {
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
    ++_stat_good_assem;
    *q_pprev = (WritablePacket *)q->next();

    click_ip *q_iph = q->ip_header();
    q_iph->ip_len = htons(q->network_length());
    q_iph->ip_sum = 0;
    q_iph->ip_sum = click_in_cksum((const unsigned char *)q_iph, q_iph->ip_hl << 2);

    // zero out the annotations we used
    memset(&PACKET_CHUNK(q), 0, sizeof(ChunkLink));
    q->set_timestamp_anno(p_in->timestamp_anno());
    q->set_next(0);

    p_in->kill();
    _mem_used -= IPH_MEM_USED + q->transport_length();
    return q;
}

void
IPReassembler::make_queue(Packet *p, WritablePacket **q_pprev)
{
    int p_off = IP_BYTE_OFF(p->ip_header());
    int p_lastoff = p_off + PACKET_DLEN(p);
    WritablePacket *q;

    if (p_off == 0) {
	q = p->uniqueify();
	if (!q) {
	    click_chatter("out of memory");
	    return;
	}
    } else {
	q = Packet::make(p->headroom() + p->ip_header_offset(), 0, 20 + p_lastoff, 0);
	if (!q) {
	    p->kill();
	    click_chatter("out of memory");
	    return;
	}
	q->set_ip_header((click_ip *)q->data(), 20);
	memcpy(q->ip_header(), p->ip_header(), 20);
	// copy data
	memcpy(q->transport_header() + p_off, p->transport_header(), PACKET_DLEN(p));
	p->kill();
    }

    _mem_used += IPH_MEM_USED + p_lastoff;

    click_ip *q_iph = q->ip_header();
    q_iph->ip_off = (q_iph->ip_off & ~htons(IP_OFFMASK)); // leave MF, DF, RF

    if (_mtu_anno >= 0)
	q->set_anno_u16(_mtu_anno, q->network_length());

    PACKET_CHUNK(q).off = p_off;
    PACKET_CHUNK(q).lastoff = p_lastoff;

    // link it up
    q->set_next(*q_pprev);
    *q_pprev = q;

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
    assert(p->has_network_header());
    const click_ip *iph = p->ip_header();
    if (!IP_ISFRAG(iph))
	return p;

    ++_stat_frags_seen;

    // reap if necessary
    int now = p->timestamp_anno().sec();
    if (!now) {
	p->timestamp_anno().assign_now();
	now = p->timestamp_anno().sec();
    }
    if (now >= _reap_time)
	reap(now);

    // calculate packet edges
    int p_off = IP_BYTE_OFF(iph);
    int p_lastoff = p_off + ntohs(iph->ip_len) - (iph->ip_hl << 2);

    // check uncommon, but annoying, case: bad length, bad length + offset,
    // or middle fragment length not a multiple of 8 bytes
    if (p_lastoff > 0xFFFF || p_lastoff <= p_off
	|| ((p_lastoff & 7) != 0 && (iph->ip_off & htons(IP_MF)) != 0)
	|| PACKET_DLEN(p) < p_lastoff - p_off) {
	p->kill();
	++_stat_bad_pkts;
	return 0;
    }
    p->take(PACKET_DLEN(p) - (p_lastoff - p_off));

    // otherwise, we need to keep the packet

    // clean up memory if necessary
    if (_mem_used > _mem_high_thresh)
	reap_overfull(now);

    // get its Packet queue
    WritablePacket **q_pprev;
    WritablePacket *q = find_queue(p, &q_pprev);
    if (!q) {			// make a new queue
	make_queue(p, q_pprev);
	return 0;
    }
    WritablePacket *q_bucket_next = (WritablePacket *)(q->next());

    if (_mtu_anno >= 0 && q->anno_u16(_mtu_anno) < p->network_length())
	q->set_anno_u16(_mtu_anno, p->network_length());

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
	    _mem_used -= IPH_MEM_USED + old_transport_length;
	    p->kill();
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
    ChunkLink *chunk = &PACKET_CHUNK(q);
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
	uint16_t old_ip_off = q->ip_header()->ip_off;
	int header_delta = p->ip_header_offset() - q->ip_header_offset();
	if (header_delta > 0)
	    q = q->push(header_delta);
	else if (header_delta < 0)
	    q->pull(-header_delta);
	q->set_ip_header((click_ip *)(q->data() + p->ip_header_offset()), p->ip_header_length());
        if (p->has_mac_header())
	    q->set_mac_header((q->data() + p->mac_header_offset()), p->mac_header_length());
	memcpy(q->data(), p->data(), p->ip_header_offset() + p->ip_header_length());
	q->ip_header()->ip_off = old_ip_off;
	ChunkLink old_chunk = PACKET_CHUNK(q);
	if (_mtu_anno >= 0) {
	    uint16_t old_mtu = q->anno_u16(_mtu_anno);
	    q->copy_annotations(p);
	    q->set_anno_u16(_mtu_anno, old_mtu);
	} else {
	    q->copy_annotations(p);
	}
	PACKET_CHUNK(q) = old_chunk;
    }

    // clear MF if incoming packet has it cleared
    if (!(iph->ip_off & htons(IP_MF)))
	q->ip_header()->ip_off &= ~htons(IP_MF);

    // Are we done with this packet?
    if ((q->ip_header()->ip_off & htons(IP_MF)) == 0
	&& PACKET_CHUNK(q).off == 0
	&& PACKET_CHUNK(q).lastoff == q->transport_length())
	return emit_whole_packet(q, q_pprev, p);

    // Otherwise, done for now
    //check();
    p->kill();
    return 0;
}

void
IPReassembler::reap_overfull(int now)
{
    check();

    // First throw away fragments at least 10 seconds old, then at least 5
    // seconds old, then any fragments.
    for (int delta = 10; delta >= 0; delta -= 5)
	for (int bucket = 0; bucket < NMAP; bucket++) {
	    WritablePacket **pprev = &_map[bucket];
	    for (WritablePacket *q = *pprev; q; q = *pprev)
		if (!delta || q->timestamp_anno().sec() < now - delta) {
		    *pprev = (WritablePacket *)q->next();
		    _mem_used -= IPH_MEM_USED + q->transport_length();
		    q->set_next(0);
		    checked_output_push(1, q);
		    ++_stat_failed_assem;
		    if (_mem_used <= _mem_low_thresh)
			return;
		} else
		    pprev = (WritablePacket **)&q->next();
	}

    click_chatter("IPReassembler: cannot free enough memory!");
}

void
IPReassembler::reap(int now)
{
    // look at all queues. If no activity for 30 seconds, kill that queue

    int kill_time = now - REAP_TIMEOUT;

    for (int i = 0; i < NMAP; i++) {
	WritablePacket **q_pprev = &_map[i];
	for (WritablePacket *q = *q_pprev; q; ) {
	    if (q->timestamp_anno().sec() < kill_time) {
		*q_pprev = (WritablePacket *)q->next();
		q->set_next(0);
		_mem_used -= IPH_MEM_USED + q->transport_length();
		checked_output_push(1, q);
	    } else
		q_pprev = (WritablePacket **)&q->next();
	    q = *q_pprev;
	}
    }

    _reap_time = now + REAP_INTERVAL;
}

void
IPReassembler::add_handlers()
{
    add_read_handler("dump", debug_dump);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPReassembler)
