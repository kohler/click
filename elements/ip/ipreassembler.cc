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
    _mem_used = 0;
    for (int i = 0; i < NMAP; i++)
	_map[i] = 0;
    static_assert(sizeof(PacketLink) <= Packet::USER_ANNO_SIZE);
}

IPReassembler::~IPReassembler()
{
    MOD_DEC_USE_COUNT;
}

int
IPReassembler::initialize(ErrorHandler *)
{
    _expire_timer.initialize(this);
    _expire_timer.schedule_after_ms(EXPIRE_TIMER_INTERVAL_MS);
    return 0;
}

void
IPReassembler::cleanup(CleanupStage)
{
    for (int i = 0; i < NMAP; i++)
	while (_map[i])
	    clean_queue(_map[i], &_map[i]);
}

void
IPReassembler::clean_queue(Packet *q, Packet **q_pprev)
{
    *q_pprev = PACKET_LINK(q)->bucket_next;
    while (q) {
	Packet *next = PACKET_LINK(q)->next;
	q->kill();
	q = next;
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
	sa << iph->ip_src << " > " << iph->ip_dst << " [" << ntohs(iph->ip_id) << '@' << IP_BYTE_OFF(iph) << '+' << PACKET_DLEN(p) << ((iph->ip_off & htons(IP_MF)) ? "+]: " : "]: ");
    sa << format;
    errh->verror(ErrorHandler::ERR_ERROR, String(), sa.cc(), val);
    va_end(val);
}

int
IPReassembler::check(ErrorHandler *errh)
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    //errh->message("------");
    for (int b = 0; b < NMAP; b++)
	for (Packet *q = _map[b]; q; q = PACKET_LINK(q)->bucket_next)
	    if (const click_ip *qip = q->ip_header()) {
		if (bucketno(qip) != b)
		    check_error(errh, b, q, "in wrong bucket");
		int prev_pos = 0;
		for (Packet *p = q; p; p = PACKET_LINK(p)->next) {
		    const click_ip *pip = p->ip_header();
		    //check_error(errh, b, p, "OK");
		    int pos = IP_BYTE_OFF(pip);
		    int endpos = pos + PACKET_DLEN(p);
		    if (PACKET_DLEN(p) > (int32_t)(ntohs(pip->ip_len) - (pip->ip_hl << 2)) || PACKET_DLEN(p) <= 0)
			check_error(errh, b, p, "odd anno length %d", PACKET_DLEN(p));
		    if (endpos > prev_pos && p != q)
			check_error(errh, b, p, "overlapping segments");
		    if ((pip->ip_off & htons(IP_MF)) == 0 && p != q)
			check_error(errh, b, p, "MF on intermediate segment");
		    if (!same_segment(pip, qip))
			check_error(errh, b, p, "on chain of wrong segment");
		    prev_pos = pos;
		}
	    } else
		errh->error("buck %d: missing IP header", b);
    return 0;
}

Packet *
IPReassembler::find_queue(Packet *p, Packet ***store_pprev)
{
    const click_ip *iph = p->ip_header();
    check();
    int bucket = bucketno(iph);
    Packet **pprev = &_map[bucket];
    Packet *q;
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
IPReassembler::emit_whole_packet(Packet *q, Packet **q_pprev, Packet *p_in,
				  Packet *first_packet)
{
    const click_ip *first_iph = first_packet->ip_header();
    int lastoff = IP_BYTE_OFF(q->ip_header()) + PACKET_DLEN(q);
    assert(IP_BYTE_OFF(first_iph) == 0);
    
    WritablePacket *m = Packet::make((const unsigned char *)0, lastoff + (first_iph->ip_hl << 2));
    if (!m) {
	click_chatter("out of memory");
	clean_queue(q, q_pprev);
	return 0;
    }

    m->set_network_header(m->data(), first_iph->ip_hl << 2);
    memcpy(m->ip_header(), first_iph, first_iph->ip_hl << 2);
    click_ip *m_iph = m->ip_header();
    m_iph->ip_off = first_iph->ip_off & ~htons(IP_OFFMASK | IP_MF);
    m_iph->ip_len = htons(lastoff + (m_iph->ip_hl << 2));
    m_iph->ip_sum = 0;
    m_iph->ip_sum = click_in_cksum((const unsigned char *)m_iph, m_iph->ip_hl << 2);

    m->copy_annotations(first_packet);
    // zero out the annotations we used
    memset(&PACKET_LINK(m)->next, 0, sizeof(struct PacketLink) - offsetof(struct PacketLink, next));
    m->set_timestamp_anno(p_in->timestamp_anno());

    *q_pprev = PACKET_LINK(q)->bucket_next;
    for (Packet *p = q; p; ) {
	assert(IP_BYTE_OFF(p->ip_header()) + PACKET_DLEN(p) == lastoff);
	lastoff -= PACKET_DLEN(p);
	memcpy(m->transport_header() + lastoff, p->transport_header(), PACKET_DLEN(p));
	Packet *next = PACKET_LINK(p)->next;
	p->kill();
	p = next;
    }

    return m;
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

#if 0
    // clean up memory if necessary
    if (_mem_used > IPFRAG_HIGH_THRESH)
	queue_evictor();
#endif

    // get its Packet queue
    Packet **q_pprev;
    Packet *q = find_queue(p, &q_pprev);
    if (!q) {			// make a new queue
	PACKET_LINK(p)->bucket_next = *q_pprev;
	PACKET_LINK(p)->next = 0;
	*q_pprev = p;
	Q_PACKET_JIFFIES(p) = click_jiffies();
	return 0;
    }
    Packet *q_bucket_next = PACKET_LINK(q)->bucket_next;

    // traverse the queue, looking for the right place to insert 'p'
    // notation: greater offsets -- [LAST_OFF.......OFF) -- lesser offsets
    Packet **pprev = q_pprev, *trav = q;
    while (trav) {
	int trav_off = IP_BYTE_OFF(trav->ip_header());
	int trav_lastoff = trav_off + PACKET_DLEN(trav);
	if (p_off >= trav_lastoff) {
	    // [----p----)[----trav----); insert p here
	    break;
	} else if (p_lastoff > trav_off) { // packets overlap
	    if (p_lastoff >= trav_lastoff && trav_off >= p_off) {
		//    [--trav--)
		// [------p-------); free trav, try again
		*pprev = PACKET_LINK(trav)->next;
		trav->kill();
		trav = *pprev;
	    } else if (trav_lastoff >= p_lastoff && p_off >= trav_off) {
		// [-----trav-----)
		//    [---p---); free p
		p->kill();
		return 0;
	    } else if (p_lastoff > trav_lastoff) {
		//     [-----trav-----)
		// [------p-------); chop trav's length, try again
		trav->take(trav_lastoff - p_off);
	    } else {
		assert(trav_lastoff > p_lastoff);
		// [----trav----)
		//    [-------p------); chop p's length, try again
		p->take(p_lastoff - trav_off);
		p_lastoff = trav_off;
	    }
	} else {
	    // [----trav----)[----p----); move past 'trav'
	    pprev = &PACKET_LINK(trav)->next;
	    trav = *pprev;
	}
    }

    // Insert 'p' after '*pprev' and before 'trav'.
    *pprev = p;
    PACKET_LINK(p)->next = trav;

    // Check whether the queue changed.
    if (*q_pprev != q) {
	assert(*q_pprev == p);
	PACKET_LINK(p)->bucket_next = q_bucket_next;
	q = p;
	// It is an error to add a fragment after the last.
	if ((trav = PACKET_LINK(q)->next)
	    && (trav->ip_header()->ip_off & htons(IP_MF)) == 0) {
	    clean_queue(q, q_pprev);
	    return 0;
	}
    }

    // Are we done with this packet?
    if ((q->ip_header()->ip_off & htons(IP_MF)) == 0) {
	// search for a hole
	Packet *first_packet = q;
	int prev_off = IP_BYTE_OFF(q->ip_header());
	for (trav = PACKET_LINK(q)->next; trav; trav = PACKET_LINK(trav)->next) {
	    int off = IP_BYTE_OFF(trav->ip_header());
	    if (off + PACKET_DLEN(trav) != prev_off)
		goto hole;
	    first_packet = trav;
	    prev_off = off;
	}
	if (prev_off == 0)
	    return emit_whole_packet(q, q_pprev, p, first_packet);
    }

    // Otherwise, done for now
  hole:
    // mark the queue packet with the current time
    Q_PACKET_JIFFIES(q) = click_jiffies();
    //check();
    return 0;
}


#if 0
void
IPReassembler::queue_evictor()
{
  int i, progress;

 restart:
  progress = 0;
  
  for(i=0; i< NMAP; i++) {
    struct IPQueue *qp;

    // if we've cleared enough memory, exit
    if (_mem_used <= IPFRAG_LOW_THRESH)
      return;

    qp = _map[i];
    if (qp) {
      while (qp->next) // find the last IPQueue
	qp = qp->next;

      queue_free(qp, 1); // free that one

      progress = 1;
    }
  }
  if (progress)
    goto restart;

  click_chatter(" panic! queue_evictor: memcount");

}
#endif


void
IPReassembler::expire_hook(Timer *, void *thunk)
{
    // look at all queues. If no activity for 30 seconds, kill that queue

    IPReassembler *ipr = reinterpret_cast<IPReassembler *>(thunk);
    int kill_time = click_jiffies() - EXPIRE_TIMEOUT * CLICK_HZ;

    for (int i = 0; i < NMAP; i++) {
	Packet **q_pprev = &ipr->_map[i];
	for (Packet *q = *q_pprev; q; ) {
	    if (Q_PACKET_JIFFIES(q) < kill_time)
		clean_queue(q, q_pprev);
	    else
		q_pprev = &PACKET_LINK(q)->bucket_next;
	    q = *q_pprev;
	}
    }

    ipr->_expire_timer.schedule_after_ms(EXPIRE_TIMER_INTERVAL_MS);
}

EXPORT_ELEMENT(IPReassembler)
