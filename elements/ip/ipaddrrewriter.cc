/*
 * ipaddrrewriter.{cc,hh} -- rewrites packet source and destination
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "iprw.hh"
#include "ipaddrrewriter.hh"
#include <clicknet/ip.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/llrpc.h>
#include <click/router.hh>
CLICK_DECLS

void
IPAddrRewriter::IPAddrMapping::apply(WritablePacket *p)
{
    click_ip *iph = p->ip_header();
    assert(iph);

    // IP header
    if (is_primary())
	iph->ip_src = _mapto.saddr();
    else {
	iph->ip_dst = _mapto.daddr();
	if (_flags & F_DST_ANNO)
	    p->set_dst_ip_anno(_mapto.daddr());
    }

    uint32_t sum = (~iph->ip_sum & 0xFFFF) + _ip_csum_delta;
    sum = (sum & 0xFFFF) + (sum >> 16);
    iph->ip_sum = ~(sum + (sum >> 16));

    mark_used();
}

String
IPAddrRewriter::IPAddrMapping::unparse() const
{
    IPFlowID rev_rev = reverse()->flow_id().rev();
    StringAccum sa;
    if (is_primary())
	sa << '(' << rev_rev.saddr() << ", -) => (" << flow_id().saddr() << ", -) [";
    else
	sa << "(-, " << rev_rev.daddr() << ") => (-, " << flow_id().daddr() << ") [";
    sa << output() << ']';
    return sa.take_string();
}

IPAddrRewriter::IPAddrRewriter()
    : _map(0), _timer(this)
{
}

IPAddrRewriter::~IPAddrRewriter()
{
    assert(!_timer.scheduled());
}

void *
IPAddrRewriter::cast(const char *n)
{
    if (strcmp(n, "IPRw") == 0)
	return (IPRw *)this;
    else if (strcmp(n, "IPAddrRewriter") == 0)
	return (IPAddrRewriter *)this;
    else
	return 0;
}

int
IPAddrRewriter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() != ninputs())
	return errh->error("need %d arguments, one per input port", ninputs());

    int before = errh->nerrors();
    for (int i = 0; i < conf.size(); i++) {
	InputSpec is;
	if (parse_input_spec(conf[i], is, "input spec " + String(i), errh) >= 0)
	    _input_specs.push_back(is);
    }
    return (errh->nerrors() == before ? 0 : -1);
}

int
IPAddrRewriter::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_msec(GC_INTERVAL_SEC * 1000);
    return 0;
}

void
IPAddrRewriter::cleanup(CleanupStage)
{
    clear_map(_map);

    for (int i = 0; i < _input_specs.size(); i++)
	if (_input_specs[i].kind == INPUT_SPEC_PATTERN)
	    _input_specs[i].u.pattern.p->unuse();
    _input_specs.clear();
}

int
IPAddrRewriter::notify_pattern(Pattern *p, ErrorHandler *errh)
{
    if (!p->allow_nat())
	return errh->error("IPAddrRewriter cannot accept IPRewriter patterns");
    else if (p->daddr())
	return errh->error("IPAddrRewriter pattern destination address must be '-'");
    return IPRw::notify_pattern(p, errh);
}

void
IPAddrRewriter::run_timer(Timer *)
{
    clean_map(_map, GC_INTERVAL_SEC * 1000);
    _timer.schedule_after_msec(GC_INTERVAL_SEC * 1000);
}

IPAddrRewriter::IPAddrMapping *
IPAddrRewriter::apply_pattern(Pattern *pattern, int,
			      const IPFlowID &in_flow, int fport, int rport)
{
    assert(fport >= 0 && fport < noutputs() && rport >= 0 && rport < noutputs());

    IPFlowID flow(in_flow.saddr(), 0, 0, 0);
    IPAddrMapping *forward = new IPAddrMapping(true);
    IPAddrMapping *reverse = new IPAddrMapping(true);

    if (forward && reverse) {
	if (!pattern)
	    Mapping::make_pair(0, flow, flow, fport, rport, forward, reverse);
	else if (!pattern->create_mapping(0, flow, fport, rport, forward, reverse, _map))
	    goto failure;

	IPFlowID reverse_flow = forward->flow_id().rev();
	_map.set(flow, forward);
	_map.set(reverse_flow, reverse);
	return forward;
    }

  failure:
    delete forward;
    delete reverse;
    return 0;
}

void
IPAddrRewriter::push(int port, Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    click_ip *iph = p->ip_header();
  
    IPFlowID flow(iph->ip_src, 0, IPAddress(), 0);
    IPAddrMapping *m = static_cast<IPAddrMapping *>(_map.get(flow));

    if (!m) {
	IPFlowID rflow = IPFlowID(IPAddress(), 0, iph->ip_dst, 0);
	m = static_cast<IPAddrMapping *>(_map.get(rflow));
    }
  
    if (!m) {			// create new mapping
	const InputSpec &is = _input_specs[port];
	switch (is.kind) {

	  case INPUT_SPEC_NOCHANGE:
	    output(is.u.output).push(p);
	    return;

	  case INPUT_SPEC_DROP:
	    break;

	  case INPUT_SPEC_KEEP:
	  case INPUT_SPEC_PATTERN: {
	      Pattern *pat = is.u.pattern.p;
	      int fport = is.u.pattern.fport;
	      int rport = is.u.pattern.rport;
	      m = IPAddrRewriter::apply_pattern(pat, 0, flow, fport, rport);
	      break;
	  }

	  case INPUT_SPEC_MAPPER: {
	      m = static_cast<IPAddrMapping *>(is.u.mapper->get_map(this, 0, flow, p));
	      break;
	  }
     
	}
	if (!m) {
	    p->kill();
	    return;
	}
    }
  
    m->apply(p);
    output(m->output()).push(p);
}


String
IPAddrRewriter::dump_mappings_handler(Element *e, void *)
{
    IPAddrRewriter *rw = (IPAddrRewriter *)e;

    StringAccum sa;
    for (Map::iterator iter = rw->_map.begin(); iter.live(); iter++) {
	Mapping *m = iter.value();
	if (m->is_primary())
	    sa << m->unparse() << "\n";
    }
    return sa.take_string();
}

String
IPAddrRewriter::dump_nmappings_handler(Element *e, void *)
{
    IPAddrRewriter *rw = (IPAddrRewriter *)e;
    return String(rw->_map.size());
}

String
IPAddrRewriter::dump_patterns_handler(Element *e, void *)
{
    IPAddrRewriter *rw = (IPAddrRewriter *)e;
    String s;
    for (int i = 0; i < rw->_input_specs.size(); i++)
	if (rw->_input_specs[i].kind == INPUT_SPEC_PATTERN)
	    s += rw->_input_specs[i].u.pattern.p->unparse() + "\n";
    return s;
}

void
IPAddrRewriter::add_handlers()
{
    add_read_handler("mappings", dump_mappings_handler, (void *)0);
    add_read_handler("nmappings", dump_nmappings_handler, (void *)0);
    add_read_handler("patterns", dump_patterns_handler, (void *)0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRw IPRewriterPatterns)
EXPORT_ELEMENT(IPAddrRewriter)
