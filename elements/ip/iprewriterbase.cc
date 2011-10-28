// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * iprewriterbase.{cc,hh} -- rewrites packet source and destination
 * Eddie Kohler
 * original versions by Eddie Kohler and Max Poletto
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
#include "iprewriterbase.hh"
#include "elements/ip/iprwpatterns.hh"
#include "elements/ip/iprwmapping.hh"
#include "elements/ip/iprwpattern.hh"
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/llrpc.h>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/algorithm.hh>
#include <click/heap.hh>

#ifdef CLICK_LINUXMODULE
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
# include <asm/softirq.h>
#endif
#include <net/sock.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#endif

CLICK_DECLS

//
// IPMapper
//

void
IPMapper::notify_rewriter(IPRewriterBase *, IPRewriterInput *, ErrorHandler *)
{
}

int
IPMapper::rewrite_flowid(IPRewriterInput *, const IPFlowID &, IPFlowID &,
			 Packet *, int)
{
    return IPRewriterBase::rw_drop;
}

//
// IPRewriterBase
//

IPRewriterBase::IPRewriterBase()
    : _map(0), _heap(new IPRewriterHeap), _gc_timer(gc_timer_hook, this)
{
    _timeouts[0] = default_timeout;
    _timeouts[1] = default_guarantee;
    _gc_interval_sec = default_gc_interval;
}

IPRewriterBase::~IPRewriterBase()
{
    if (_heap)
	_heap->unuse();
}


int
IPRewriterBase::parse_input_spec(const String &line, IPRewriterInput &is,
				 int input_number, ErrorHandler *errh)
{
    PrefixErrorHandler cerrh(errh, "input spec " + String(input_number) + ": ");
    String word, rest;
    if (!cp_word(line, &word, &rest))
	return cerrh.error("empty argument");
    cp_eat_space(rest);

    is.kind = IPRewriterInput::i_drop;
    is.owner = this;
    is.owner_input = input_number;
    is.reply_element = this;

    if (word == "pass" || word == "passthrough" || word == "nochange") {
	int32_t outnum = 0;
	if (rest && !IntArg().parse(rest, outnum))
	    return cerrh.error("syntax error, expected %<nochange [OUTPUT]%>");
	else if ((unsigned) outnum >= (unsigned) noutputs())
	    return cerrh.error("output port out of range");
	is.kind = IPRewriterInput::i_nochange;
	is.foutput = outnum;

    } else if (word == "keep") {
	Vector<String> words;
	cp_spacevec(rest, words);
	if (!IPRewriterPattern::parse_ports(words, &is, this, &cerrh))
	    return -1;
	if ((unsigned) is.foutput >= (unsigned) noutputs()
	    || (unsigned) is.routput >= (unsigned) is.reply_element->noutputs())
	    return cerrh.error("output port out of range");
	is.kind = IPRewriterInput::i_keep;

    } else if (word == "drop" || word == "discard") {
	if (rest)
	    return cerrh.error("syntax error, expected %<%s%>", word.c_str());

    } else if (word == "pattern" || word == "xpattern") {
	if (!IPRewriterPattern::parse_with_ports(rest, &is, this, &cerrh))
	    return -1;
	if ((unsigned) is.foutput >= (unsigned) noutputs()
	    || (unsigned) is.routput >= (unsigned) is.reply_element->noutputs())
	    return cerrh.error("output port out of range");
	is.u.pattern->use();
	is.kind = IPRewriterInput::i_pattern;

    } else if (Element *e = cp_element(word, this, 0)) {
	IPMapper *mapper = (IPMapper *)e->cast("IPMapper");
	if (rest)
	    return cerrh.error("syntax error, expected element name");
	else if (!mapper)
	    return cerrh.error("element is not an IPMapper");
	else {
	    is.kind = IPRewriterInput::i_mapper;
	    is.u.mapper = mapper;
	}

    } else
	return cerrh.error("unknown specification");

    return 0;
}

int
IPRewriterBase::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String capacity_word;

    if (Args(this, errh).bind(conf)
	.read("CAPACITY", AnyArg(), capacity_word)
	.read("MAPPING_CAPACITY", AnyArg(), capacity_word)
	.read("TIMEOUT", SecondsArg(), _timeouts[0])
	.read("GUARANTEE", SecondsArg(), _timeouts[1])
	.read("REAP_INTERVAL", SecondsArg(), _gc_interval_sec)
	.read("REAP_TIME", Args::deprecated, SecondsArg(), _gc_interval_sec)
	.consume() < 0)
	return -1;

    if (capacity_word) {
	Element *e;
	IPRewriterBase *rwb;
	if (IntArg().parse(capacity_word, _heap->_capacity))
	    /* OK */;
	else if ((e = cp_element(capacity_word, this))
		 && (rwb = (IPRewriterBase *) e->cast("IPRewriterBase"))) {
	    rwb->_heap->use();
	    _heap->unuse();
	    _heap = rwb->_heap;
	} else
	    return errh->error("bad MAPPING_CAPACITY");
    }

    if (conf.size() != ninputs())
	return errh->error("need %d arguments, one per input port", ninputs());

    _timeouts[0] *= CLICK_HZ;	// _timeouts is measured in jiffies
    _timeouts[1] *= CLICK_HZ;

    for (int i = 0; i < conf.size(); ++i) {
	IPRewriterInput is;
	if (parse_input_spec(conf[i], is, i, errh) >= 0)
	    _input_specs.push_back(is);
    }

    return _input_specs.size() == ninputs() ? 0 : -1;
}

int
IPRewriterBase::initialize(ErrorHandler *errh)
{
    for (int i = 0; i < _input_specs.size(); ++i) {
	PrefixErrorHandler cerrh(errh, "input spec " + String(i) + ": ");
	if (_input_specs[i].reply_element->_heap != _heap)
	    cerrh.error("reply element %<%s%> must share this MAPPING_CAPACITY", i, _input_specs[i].reply_element->name().c_str());
	if (_input_specs[i].kind == IPRewriterInput::i_mapper)
	    _input_specs[i].u.mapper->notify_rewriter(this, &_input_specs[i], &cerrh);
    }
    _gc_timer.initialize(this);
    if (_gc_interval_sec)
	_gc_timer.schedule_after_sec(_gc_interval_sec);
    return errh->nerrors() ? -1 : 0;
}

void
IPRewriterBase::cleanup(CleanupStage)
{
    shrink_heap(true);
    for (int i = 0; i < _input_specs.size(); ++i)
	if (_input_specs[i].kind == IPRewriterInput::i_pattern)
	    _input_specs[i].u.pattern->unuse();
    _input_specs.clear();
}

IPRewriterEntry *
IPRewriterBase::get_entry(int ip_p, const IPFlowID &flowid, int input)
{
    IPRewriterEntry *m = _map.get(flowid);
    if (m && ip_p && m->flow()->ip_p() && m->flow()->ip_p() != ip_p)
	return 0;
    if (!m && (unsigned) input < (unsigned) _input_specs.size()) {
	IPRewriterInput &is = _input_specs[input];
	IPFlowID rewritten_flowid = IPFlowID::uninitialized_t();
	if (is.rewrite_flowid(flowid, rewritten_flowid, 0) == rw_addmap)
	    m = add_flow(ip_p, flowid, rewritten_flowid, input);
    }
    return m;
}

IPRewriterEntry *
IPRewriterBase::store_flow(IPRewriterFlow *flow, int input,
			   Map &map, Map *reply_map_ptr)
{
    IPRewriterBase *reply_element = _input_specs[input].reply_element;
    if ((unsigned) flow->entry(false).output() >= (unsigned) noutputs()
	|| (unsigned) flow->entry(true).output() >= (unsigned) reply_element->noutputs()) {
	flow->owner()->owner->destroy_flow(flow);
	return 0;
    }

    IPRewriterEntry *old = map.set(&flow->entry(false));
    assert(!old);

    if (!reply_map_ptr)
	reply_map_ptr = &reply_element->_map;
    old = reply_map_ptr->set(&flow->entry(true));
    if (unlikely(old)) {		// Assume every map has the same heap.
	if (likely(old->flow() != flow))
	    old->flow()->destroy(_heap);
    }

    Vector<IPRewriterFlow *> &myheap = _heap->_heaps[flow->guaranteed()];
    myheap.push_back(flow);
    push_heap(myheap.begin(), myheap.end(),
	      IPRewriterFlow::heap_less(), IPRewriterFlow::heap_place());
    ++_input_specs[input].count;

    if (unlikely(_heap->size() > _heap->capacity())) {
	// This may destroy the newly added mapping, if it has the lowest
	// expiration time.  How can we tell?  If (1) flows are added to the
	// heap one at a time, so the heap was formerly no bigger than the
	// capacity, and (2) 'flow' expires in the future, then we will only
	// destroy 'flow' if it's the top of the heap.
	click_jiffies_t now_j = click_jiffies();
	assert(click_jiffies_less(now_j, flow->expiry())
	       && _heap->size() == _heap->capacity() + 1);
	if (shrink_heap_for_new_flow(flow, now_j)) {
	    ++_input_specs[input].failures;
	    return 0;
	}
    }

    if (map.unbalanced())
	map.rehash(map.bucket_count() + 1);
    if (reply_map_ptr != &map && reply_map_ptr->unbalanced())
	reply_map_ptr->rehash(reply_map_ptr->bucket_count() + 1);
    return &flow->entry(false);
}

void
IPRewriterBase::shift_heap_best_effort(click_jiffies_t now_j)
{
    // Shift flows with expired guarantees to the best-effort heap.
    Vector<IPRewriterFlow *> &guaranteed_heap = _heap->_heaps[1];
    while (guaranteed_heap.size() && guaranteed_heap[0]->expired(now_j)) {
	IPRewriterFlow *mf = guaranteed_heap[0];
	click_jiffies_t new_expiry = mf->owner()->owner->best_effort_expiry(mf);
	mf->change_expiry(_heap, false, new_expiry);
    }
}

bool
IPRewriterBase::shrink_heap_for_new_flow(IPRewriterFlow *flow,
					 click_jiffies_t now_j)
{
    shift_heap_best_effort(now_j);
    // At this point, all flows in the guarantee heap expire in the future.
    // So remove the next-to-expire best-effort flow, unless there are none.
    // In that case we always remove the current flow to honor previous
    // guarantees (= admission control).
    IPRewriterFlow *deadf;
    if (_heap->_heaps[0].empty()) {
	assert(flow->guaranteed());
	deadf = flow;
    } else
	deadf = _heap->_heaps[0][0];
    deadf->destroy(_heap);
    return deadf == flow;
}

void
IPRewriterBase::shrink_heap(bool clear_all)
{
    click_jiffies_t now_j = click_jiffies();
    shift_heap_best_effort(now_j);
    Vector<IPRewriterFlow *> &best_effort_heap = _heap->_heaps[0];
    while (best_effort_heap.size() && best_effort_heap[0]->expired(now_j))
	best_effort_heap[0]->destroy(_heap);

    int32_t capacity = clear_all ? 0 : _heap->_capacity;
    while (_heap->size() > capacity) {
	IPRewriterFlow *deadf = _heap->_heaps[_heap->_heaps[0].empty()][0];
	deadf->destroy(_heap);
    }
}

void
IPRewriterBase::gc_timer_hook(Timer *t, void *user_data)
{
    IPRewriterBase *rw = static_cast<IPRewriterBase *>(user_data);
    rw->shrink_heap(false);
    if (rw->_gc_interval_sec)
	t->reschedule_after_sec(rw->_gc_interval_sec);
}

String
IPRewriterBase::read_handler(Element *e, void *user_data)
{
    IPRewriterBase *rw = static_cast<IPRewriterBase *>(e);
    intptr_t what = reinterpret_cast<intptr_t>(user_data);
    StringAccum sa;

    switch (what) {
    case h_nmappings: {
	uint32_t count = 0;
	for (int i = 0; i < rw->_input_specs.size(); ++i)
	    count += rw->_input_specs[i].count;
	sa << count;
	break;
    }
    case h_mapping_failures: {
	uint32_t count = 0;
	for (int i = 0; i < rw->_input_specs.size(); ++i)
	    count += rw->_input_specs[i].failures;
	sa << count;
	break;
    }
    case h_size:
	sa << rw->_heap->size();
	break;
    case h_capacity:
	sa << rw->_heap->_capacity;
	break;
    default:
	for (int i = 0; i < rw->_input_specs.size(); ++i) {
	    if (what != h_patterns && what != i)
		continue;
	    switch (rw->_input_specs[i].kind) {
	    case IPRewriterInput::i_drop:
		sa << "<drop>";
		break;
	    case IPRewriterInput::i_nochange:
		sa << "<nochange>";
		break;
	    case IPRewriterInput::i_keep:
		sa << "<keep>";
		break;
	    case IPRewriterInput::i_pattern:
		sa << rw->_input_specs[i].u.pattern->unparse();
		break;
	    case IPRewriterInput::i_mapper:
		sa << "<mapper>";
		break;
	    }
	    if (rw->_input_specs[i].count)
		sa << " [" << rw->_input_specs[i].count << ']';
	    sa << '\n';
	}
	break;
    }
    return sa.take_string();
}

int
IPRewriterBase::write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh)
{
    IPRewriterBase *rw = static_cast<IPRewriterBase *>(e);
    intptr_t what = reinterpret_cast<intptr_t>(user_data);
    if (what == h_capacity) {
	if (Args(e, errh).push_back_words(str)
	    .read_mp("CAPACITY", rw->_heap->_capacity)
	    .complete() < 0)
	    return -1;
	rw->shrink_heap(false);
	return 0;
    } else if (what == h_clear) {
	rw->shrink_heap(true);
	return 0;
    } else
	return -1;
}

int
IPRewriterBase::pattern_write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh)
{
    IPRewriterBase *rw = static_cast<IPRewriterBase *>(e);
    intptr_t what = reinterpret_cast<intptr_t>(user_data);
    IPRewriterInput is;
    int r = rw->parse_input_spec(str, is, what, errh);
    if (r >= 0) {
	IPRewriterInput *spec = &rw->_input_specs[what];

	// remove all existing flows created by this input
	for (int which_heap = 0; which_heap < 2; ++which_heap) {
	    Vector<IPRewriterFlow *> &myheap = rw->_heap->_heaps[which_heap];
	    for (int i = myheap.size() - 1; i >= 0; --i)
		if (myheap[i]->owner() == spec) {
		    myheap[i]->destroy(rw->_heap);
		    if (i < myheap.size())
			++i;
		}
	}

	// change pattern
	if (spec->kind == IPRewriterInput::i_pattern)
	    spec->u.pattern->unuse();
	*spec = is;
    }
    return 0;
}

void
IPRewriterBase::add_rewriter_handlers(bool writable_patterns)
{
    add_read_handler("nmappings", read_handler, h_nmappings);
    add_read_handler("mapping_failures", read_handler, h_mapping_failures);
    add_read_handler("patterns", read_handler, h_patterns);
    add_read_handler("size", read_handler, h_size);
    add_read_handler("capacity", read_handler, h_capacity);
    add_write_handler("capacity", write_handler, h_capacity);
    add_write_handler("clear", write_handler, h_clear);
    for (int i = 0; i < ninputs(); ++i) {
	String name = "pattern" + String(i);
	add_read_handler(name, read_handler, i);
	if (writable_patterns)
	    add_write_handler(name, pattern_write_handler, i, Handler::EXCLUSIVE);
    }
}

int
IPRewriterBase::llrpc(unsigned command, void *data)
{
    if (command == CLICK_LLRPC_IPREWRITER_MAP_TCP) {
	// Data	: unsigned saddr, daddr; unsigned short sport, dport
	// Incoming : the flow ID
	// Outgoing : If there is a mapping for that flow ID, then stores the
	//	      mapping into 'data' and returns zero. Otherwise, returns
	//	      -EAGAIN.

	IPFlowID *val = reinterpret_cast<IPFlowID *>(data);
	IPRewriterEntry *m = get_entry(IP_PROTO_TCP, *val, -1);
	if (!m)
	    return -EAGAIN;
	*val = m->rewritten_flowid();
	return 0;

    } else if (command == CLICK_LLRPC_IPREWRITER_MAP_UDP) {
	// Data	: unsigned saddr, daddr; unsigned short sport, dport
	// Incoming : the flow ID
	// Outgoing : If there is a mapping for that flow ID, then stores the
	//	      mapping into 'data' and returns zero. Otherwise, returns
	//	      -EAGAIN.

	IPFlowID *val = reinterpret_cast<IPFlowID *>(data);
	IPRewriterEntry *m = get_entry(IP_PROTO_UDP, *val, -1);
	if (!m)
	    return -EAGAIN;
	*val = m->rewritten_flowid();
	return 0;

    } else
	return Element::llrpc(command, data);
}

ELEMENT_REQUIRES(IPRewriterMapping IPRewriterPattern)
ELEMENT_PROVIDES(IPRewriterBase)
CLICK_ENDDECLS
