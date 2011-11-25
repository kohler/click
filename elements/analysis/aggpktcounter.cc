// -*- c-basic-offset: 4 -*-
/*
 * aggpktcounter.{cc,hh} -- element counts packets per packet number and
 * aggregate annotation
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2008 Meraki, Inc.
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
#include "aggpktcounter.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
CLICK_DECLS

AggregatePacketCounter::Flow::Flow(uint32_t aggregate, int columns)
    : _aggregate(aggregate), _next(0), _counts(new Vector<uint32_t>[columns])
{
}

void
AggregatePacketCounter::Flow::add(uint32_t packetno, int column)
{
    if (_counts[column].size() <= (int) packetno)
	_counts[column].resize(packetno + 1, 0);
    _counts[column].unchecked_at(packetno)++;
}

AggregatePacketCounter::packetctr_t
AggregatePacketCounter::Flow::column_count(int column) const
{
    packetctr_t count = 0;
    for (const uint32_t *v = _counts[column].begin(); v < _counts[column].end(); v++)
	count += *v;
    return count;
}

void
AggregatePacketCounter::Flow::received(Vector<uint32_t> &v, const AggregatePacketCounter *apc) const
{
    int n = 0;
    for (int port = 0; port < apc->noutputs(); port++)
	if (_counts[port].size() > n)
	    n = _counts[port].size();
    for (int packetno = 0; packetno < n; packetno++)
	for (int port = 0; port < apc->noutputs(); port++)
	    if (packetno < _counts[port].size() && _counts[port].unchecked_at(packetno)) {
		v.push_back(packetno);
		break;
	    }
}

void
AggregatePacketCounter::Flow::undelivered(Vector<uint32_t> &undelivered, const AggregatePacketCounter *apc) const
{
    assert(apc->noutputs() >= 2);
    int packetno;
    int min_n = (_counts[0].size() < _counts[1].size() ? _counts[0].size() : _counts[1].size());
    for (packetno = 0; packetno < min_n; packetno++)
	if (_counts[0].unchecked_at(packetno) > _counts[1].unchecked_at(packetno))
	    undelivered.push_back(packetno);
    for (; packetno < _counts[0].size(); packetno++)
	if (_counts[0].unchecked_at(packetno))
	    undelivered.push_back(packetno);
}


AggregatePacketCounter::AggregatePacketCounter()
{
    for (int i = 0; i < NFLOWMAP; i++)
	_flowmap[i] = 0;
}

AggregatePacketCounter::~AggregatePacketCounter()
{
}

int
AggregatePacketCounter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e = 0;
    String anno = String::make_stable("PACKET_NUMBER");

    if (Args(conf, this, errh)
	.read("NOTIFIER", e)
	.read("ANNO", WordArg(), anno)
	.complete() < 0)
	return -1;

    if (anno == "NONE")
	_anno = -1;
    else if (!cp_anno(anno, 4, &_anno, this))
	return errh->error("bad ANNO");
    /*if (e && !(_agg_notifier = (AggregateNotifier *)e->cast("AggregateNotifier")))
      return errh->error("%s is not an AggregateNotifier", e->name().c_str()); */

    return 0;
}

int
AggregatePacketCounter::initialize(ErrorHandler *)
{
    _total_flows = _total_packets = 0;
    //if (_agg_notifier)
    //_agg_notifier->add_listener(this);
    //_gc_timer.initialize(this);
    return 0;
}

void
AggregatePacketCounter::end_flow(Flow *f, ErrorHandler *)
{
    /*    if (f->npackets() >= _mincount) {
	f->output(errh);
	if (_gzip && f->filename() != "-")
	    if (add_compressable(f->filename(), errh) < 0)
		_gzip = false;
    } else
    f->unlink(errh);*/
    delete f;
}

void
AggregatePacketCounter::cleanup(CleanupStage)
{
    ErrorHandler *errh = ErrorHandler::default_handler();
    for (int i = 0; i < NFLOWMAP; i++)
	while (Flow *f = _flowmap[i]) {
	    _flowmap[i] = f->next();
	    end_flow(f, errh);
	}
    if (_total_packets > 0 && _total_flows == 0)
	errh->lwarning(declaration(), "saw no packets with aggregate annotations");
}

AggregatePacketCounter::Flow *
AggregatePacketCounter::find_flow(uint32_t agg)
{
    if (agg == 0)
	return 0;

    int bucket = (agg & (NFLOWMAP - 1));
    Flow *prev = 0, *f = _flowmap[bucket];
    while (f && f->aggregate() != agg) {
	prev = f;
	f = f->next();
    }

    if (f)
	/* nada */;
    else if ((f = new Flow(agg, ninputs()))) {
	prev = f;
	_total_flows++;
    } else
	return 0;

    if (prev) {
	prev->set_next(f->next());
	f->set_next(_flowmap[bucket]);
	_flowmap[bucket] = f;
    }

    return f;
}

inline void
AggregatePacketCounter::smaction(int port, Packet *p)
{
    _total_packets++;
    if (Flow *f = find_flow(AGGREGATE_ANNO(p))) {
	if (_anno >= 0)
	    f->add(p->anno_u32(_anno), port);
	else
	    f->add(0, port);
    }
}

void
AggregatePacketCounter::push(int port, Packet *p)
{
    smaction(port, p);
    output(port).push(p);
}

Packet *
AggregatePacketCounter::pull(int port)
{
    if (Packet *p = input(port).pull()) {
	smaction(port, p);
	return p;
    } else
	return 0;
}

/*
void
AggregatePacketCounter::aggregate_notify(uint32_t agg, AggregateEvent event, const Packet *)
{
    if (event == DELETE_AGG && find_aggregate(agg, 0)) {
	_gc_aggs.push_back(agg);
	_gc_aggs.push_back(click_jiffies());
	if (!_gc_timer.scheduled())
	    _gc_timer.schedule_after_msec(250);
    }
}

void
AggregatePacketCounter::gc_hook(Timer *t, void *thunk)
{
    AggregatePacketCounter *td = static_cast<AggregatePacketCounter *>(thunk);
    uint32_t limit_jiff = click_jiffies() - (CLICK_HZ / 4);
    int i;
    for (i = 0; i < td->_gc_aggs.size() && SEQ_LEQ(td->_gc_aggs[i+1], limit_jiff); i += 2)
	if (Flow *f = td->find_aggregate(td->_gc_aggs[i], 0)) {
	    int bucket = (f->aggregate() & (NFLOWMAP - 1));
	    assert(td->_flowmap[bucket] == f);
	    td->_flowmap[bucket] = f->next();
	    td->end_flow(f, ErrorHandler::default_handler());
	}
    if (i < td->_gc_aggs.size()) {
	td->_gc_aggs.erase(td->_gc_aggs.begin(), td->_gc_aggs.begin() + i);
	t->schedule_after_msec(250);
    }
}
*/

enum { H_CLEAR, H_COUNT };

String
AggregatePacketCounter::read_handler(Element *e, void *thunk)
{
    AggregatePacketCounter *ac = static_cast<AggregatePacketCounter *>(e);
    switch ((intptr_t)thunk) {
      case H_COUNT: {
	  packetctr_t count = 0;
	  for (int i = 0; i < NFLOWMAP; i++)
	      for (const Flow *f = ac->_flowmap[i]; f; f = f->next())
		  for (int col = 0; col < ac->ninputs(); col++)
		      count += f->column_count(col);
	  return String(count);
      }
      default:
	return "<error>";
    }
}

int
AggregatePacketCounter::write_handler(const String &, Element *e, void *thunk, ErrorHandler *errh)
{
    AggregatePacketCounter *td = static_cast<AggregatePacketCounter *>(e);
    switch ((intptr_t)thunk) {
      case H_CLEAR:
	for (int i = 0; i < NFLOWMAP; i++)
	    while (Flow *f = td->_flowmap[i]) {
		td->_flowmap[i] = f->next();
		td->end_flow(f, errh);
	    }
	return 0;
      default:
	return -1;
    }
}

String
AggregatePacketCounter::flow_handler(uint32_t aggregate, FlowFunc func)
{
    Vector<uint32_t> v;
    if (Flow *f = find_flow(aggregate))
	(f->*func)(v, this);
    StringAccum sa;
    for (int i = 0; i < v.size(); i++)
	sa << v[i] << '\n';
    return sa.take_string();
}

int
AggregatePacketCounter::thing_read_handler(int, String& s, Element* e, const Handler* h, ErrorHandler* errh)
{
    uint32_t aggregate = 0;
    if (s && !IntArg().parse(cp_uncomment(s), aggregate))
	return errh->error("argument should be aggregate number");
    FlowFunc ff = (h->read_user_data() ? &Flow::undelivered : &Flow::received);
    AggregatePacketCounter *apc = static_cast<AggregatePacketCounter *>(e);
    s = apc->flow_handler(aggregate, ff);
    return 0;
}

void
AggregatePacketCounter::add_handlers()
{
    add_write_handler("clear", write_handler, H_CLEAR);
    add_read_handler("count", read_handler, H_COUNT);
    set_handler("received", Handler::h_read | Handler::h_read_param, thing_read_handler, 0);
    set_handler("undelivered", Handler::h_read | Handler::h_read_param, thing_read_handler, (void*) 1);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel int64)
EXPORT_ELEMENT(AggregatePacketCounter)
