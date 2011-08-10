// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * iprwmapping.{cc,hh} -- helper class for IPRewriter
 * Eddie Kohler
 * original version by Max Poletto and Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2008-2010 Meraki, Inc.
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
#include "iprwmapping.hh"
#include "iprewriterbase.hh"
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/algorithm.hh>
#include <click/heap.hh>
CLICK_DECLS

IPRewriterFlow::IPRewriterFlow(IPRewriterInput *owner, const IPFlowID &flowid,
			       const IPFlowID &rewritten_flowid,
			       uint8_t ip_p, bool guaranteed,
			       click_jiffies_t expiry_j)
    : _expiry_j(expiry_j), _ip_p(ip_p), _tflags(0),
      _guaranteed(guaranteed), _reply_anno(0),
      _owner(owner)
{
    _e[0].initialize(flowid, owner->foutput, false);
    _e[1].initialize(rewritten_flowid.reverse(), owner->routput, true);

    // set checksum deltas
    const uint16_t *swords = reinterpret_cast<const uint16_t *>(&flowid);
    const uint16_t *dwords = reinterpret_cast<const uint16_t *>(&rewritten_flowid);
    _ip_csum_delta = 0;
    for (int i = 0; i < 4; ++i)
	click_update_in_cksum(&_ip_csum_delta, swords[i], dwords[i]);
    _udp_csum_delta = _ip_csum_delta;
    for (int i = 4; i < 6; ++i)
	click_update_in_cksum(&_udp_csum_delta, swords[i], dwords[i]);
}

void
IPRewriterFlow::apply(WritablePacket *p, bool direction, unsigned annos)
{
    assert(p->has_network_header());
    click_ip *iph = p->ip_header();

    // IP header
    const IPFlowID &revflow = _e[!direction].hashkey();
    iph->ip_src = revflow.daddr();
    iph->ip_dst = revflow.saddr();
    if (annos & 1)
	p->set_dst_ip_anno(revflow.saddr());
    if (direction && (annos & 2))
	p->set_anno_u8(annos >> 2, _reply_anno);
    update_csum(&iph->ip_sum, direction, _ip_csum_delta);

    // end if not first fragment
    if (!IP_FIRSTFRAG(iph))
	return;

    // UDP/TCP header
    if (iph->ip_p == IP_PROTO_TCP && p->transport_length() >= 18) {
	click_tcp *tcph = p->tcp_header();
	tcph->th_sport = revflow.dport();
	tcph->th_dport = revflow.sport();
	update_csum(&tcph->th_sum, direction, _udp_csum_delta);
    } else if (iph->ip_p == IP_PROTO_UDP) {
	click_udp *udph = p->udp_header();
	udph->uh_sport = revflow.dport();
	udph->uh_dport = revflow.sport();
	if (udph->uh_sum)	// 0 checksum is no checksum
	    update_csum(&udph->uh_sum, direction, _udp_csum_delta);
    }
}

void
IPRewriterFlow::change_expiry(IPRewriterHeap *h, bool guaranteed,
			      click_jiffies_t expiry_j)
{
    Vector<IPRewriterFlow *> &current_heap = h->_heaps[_guaranteed];
    assert(current_heap[_place] == this);
    _expiry_j = expiry_j;
    if (_guaranteed != guaranteed) {
	remove_heap(current_heap.begin(), current_heap.end(),
		    current_heap.begin() + _place,
		    heap_less(), heap_place());
	current_heap.pop_back();
	_guaranteed = guaranteed;
	Vector<IPRewriterFlow *> &new_heap = h->_heaps[_guaranteed];
	new_heap.push_back(this);
	push_heap(new_heap.begin(), new_heap.end(),
		  heap_less(), heap_place());
    } else
	change_heap(current_heap.begin(), current_heap.end(),
		    current_heap.begin() + _place,
		    heap_less(), heap_place());
}

void
IPRewriterFlow::destroy(IPRewriterHeap *heap)
{
    Vector<IPRewriterFlow *> &myheap = heap->_heaps[_guaranteed];
    remove_heap(myheap.begin(), myheap.end(), myheap.begin() + _place,
		heap_less(), heap_place());
    myheap.pop_back();
    --_owner->count;
    _owner->owner->destroy_flow(this);
}

void
IPRewriterFlow::unparse_ports(StringAccum &sa, bool direction,
			      click_jiffies_t now) const
{
    IPRewriterBase *my_e = _owner->owner, *reply_e = _owner->reply_element;
#if 1 /* UNPARSE_PORTS_OUTPUTS */
    sa << " [";
    if (!direction)
	sa << '*';
    else if (my_e != reply_e)
	sa << my_e->name() << ':';
    sa << _e[false].output() << ' ';
    if (direction)
	sa << '*';
    else if (my_e != reply_e)
	sa << reply_e->name() << ':';
    click_jiffies_t expiry_j = _expiry_j;
    if (_guaranteed)
	expiry_j = my_e->best_effort_expiry(this);
    sa << _e[true].output() << "] i" << _owner->owner_input << " exp"
       << (expiry_j + (CLICK_HZ / 2) - now) / CLICK_HZ;
#else
    sa << " [";
    if (direction && my_e != reply_e)
	sa << my_e->name() << ':';
    sa << _owner->owner_input << (direction ? " R]" : " F]");
#endif
}

void
IPRewriterFlow::unparse(StringAccum &sa, bool direction,
			click_jiffies_t now) const
{
    sa << _e[direction].flowid() << " => " << _e[direction].rewritten_flowid();
    unparse_ports(sa, direction, now);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRewriterPattern)
ELEMENT_PROVIDES(IPRewriterMapping)
