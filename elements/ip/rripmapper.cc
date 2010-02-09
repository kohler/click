/*
 * rripmapper.{cc,hh} -- round robin IPMapper
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2009-2010 Meraki, Inc.
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
#include "rripmapper.hh"
#include "elements/ip/iprwpattern.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

RoundRobinIPMapper::RoundRobinIPMapper()
{
}

RoundRobinIPMapper::~RoundRobinIPMapper()
{
}

void *
RoundRobinIPMapper::cast(const char *name)
{
  if (name && strcmp("RoundRobinIPMapper", name) == 0)
    return (Element *)this;
  else if (name && strcmp("IPMapper", name) == 0)
    return (IPMapper *)this;
  else
    return 0;
}

int
RoundRobinIPMapper::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() == 0)
	return errh->error("no patterns given");
    else if (conf.size() == 1)
	errh->warning("only one pattern given");

    int before = errh->nerrors();

    for (int i = 0; i < conf.size(); i++) {
	IPRewriterInput is;
	is.kind = IPRewriterInput::i_pattern;
	if (IPRewriterPattern::parse_with_ports(conf[i], &is, this, errh)) {
	    is.u.pattern->use();
	    _is.push_back(is);
	}
    }

    _last_pattern = 0;
    return (errh->nerrors() == before ? 0 : -1);
}

void
RoundRobinIPMapper::cleanup(CleanupStage)
{
    for (int i = 0; i < _is.size(); i++)
	_is[i].u.pattern->unuse();
}

void
RoundRobinIPMapper::notify_rewriter(IPRewriterBase *rw, ErrorHandler *errh)
{
    int no = rw->noutputs();
    for (int i = 0; i < _is.size(); i++) {
	if (_is[i].foutput >= no || _is[i].routput >= no)
	    errh->error("port in %<%s%> out of range for %<%s%>", declaration().c_str(), rw->declaration().c_str());
    }
}

int
RoundRobinIPMapper::rewrite_flowid(IPRewriterInput *input,
				   const IPFlowID &flowid,
				   IPFlowID &rewritten_flowid,
				   Packet *p, int mapid)
{
    for (int i = 0; i < _is.size(); ++i) {
	IPRewriterInput &is = _is[_last_pattern];
	++_last_pattern;
	if (_last_pattern == _is.size())
	    _last_pattern = 0;
	is.reply_element = input->reply_element;
	int result = is.rewrite_flowid(flowid, rewritten_flowid, p, mapid);
	if (result != IPRewriterBase::rw_drop
	    || is.kind == IPRewriterInput::i_drop) {
	    input->foutput = is.foutput;
	    input->routput = is.routput;
	    return result;
	}
    }
    return IPRewriterBase::rw_drop;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRewriterBase)
EXPORT_ELEMENT(RoundRobinIPMapper)
