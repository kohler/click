// -*- c-basic-offset: 4 -*-
/*
 * aggregatefilter.{cc,hh} -- count packets/bytes with given aggregate
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
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
#include "aggregatefilter.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

AggregateFilter::Group::Group(uint32_t aggregate)
    : groupno(aggregate & GROUPMASK), next(0)
{
    memset(filters, 0, sizeof(filters));
}

AggregateFilter::AggregateFilter()
    : _default_output(1)
{
    for (unsigned i = 0; i < NBUCKETS; i++)
	_groups[i] = 0;
}

AggregateFilter::~AggregateFilter()
{
}

AggregateFilter::Group *
AggregateFilter::find_group(uint32_t aggregate)
{
    int bucket = (aggregate >> GROUPSHIFT) % NBUCKETS;
    Group *g = _groups[bucket];
    uint32_t groupno = aggregate & GROUPMASK;
    while (g && g->groupno != groupno)
	g = g->next;
    if (!g && (g = new Group(aggregate))) {
	g->next = _groups[bucket];
	_groups[bucket] = g;
    }
    return g;
}

int
AggregateFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _default_output = noutputs() + 1;
    Group *g = 0;

    for (int argno = 0; argno < conf.size(); argno++) {
	Vector<String> words;
	cp_spacevec(conf[argno], words);

	if (words.size() == 0) {
	    errh->error("empty pattern %d", argno);
	    continue;
	} else if (_default_output <= noutputs()) {
	    errh->warning("pattern %d matches no packets", argno);
	    continue;
	}

	int port = noutputs();
	if (words[0] == "allow")
	    port = 0;
	else if (IntArg().parse(words[0], port) && port >= 0)
	    /* OK */;
	else if (words[0] != "drop" && words[0] != "deny") {
	    errh->error("pattern %d: expected a port number", argno);
	    continue;
	}

	if (words.size() == 1)
	    errh->error("pattern %d has no aggregates", argno);
	else if (words.size() == 2 && (words[1] == "all" || words[1] == "-"))
	    _default_output = port;
	else
	    for (int i = 1; i < words.size(); i++) {
		uint32_t agg1 = 0, agg2 = 0;
		const char *dash;
		if (IntArg().parse(words[i], agg1))
		    agg2 = agg1;
		else {
		    dash = find(words[i], '-');
		    if (!IntArg().parse(words[i].substring(words[i].begin(), dash), agg1)
			|| !IntArg().parse(words[i].substring(dash + 1, words[i].end()), agg2)) {
			errh->error("pattern %d: bad aggregate number %<%#s%>", argno, words[i].c_str());
			continue;
		    }
		}

		while (agg1 <= agg2) {
		    if (!g || g->groupno != (agg1 & GROUPMASK))
			g = find_group(agg1);
		    int which = agg1 & INGROUPMASK;
		    if (g->filters[which] && g->filters[which] != port + 1)
			errh->warning("pattern %d: aggregate %u already filtered to output %d", argno, agg1, g->filters[which] - 1);
		    else
			g->filters[which] = port + 1;
		    agg1++;
		}
	    }
    }

    return errh->nerrors() ? -1 : 0;
}

void
AggregateFilter::cleanup(CleanupStage)
{
    for (unsigned i = 0; i < NBUCKETS; i++)
	while (_groups[i]) {
	    Group *g = _groups[i];
	    _groups[i] = g->next;
	    delete g;
	}
}

void
AggregateFilter::push(int, Packet *p)
{
    // find group
    int bucket = (AGGREGATE_ANNO(p) >> GROUPSHIFT) % NBUCKETS;
    Group *g = _groups[bucket], *prev = 0;
    uint32_t groupno = AGGREGATE_ANNO(p) & GROUPMASK;
    while (g && g->groupno != groupno) {
	prev = g;
	g = g->next;
    }
    if (prev && g) {
	prev->next = g->next;
	g->next = _groups[bucket];
	_groups[bucket] = g;
    }

    // output packet appropriately
    uint32_t eltno = AGGREGATE_ANNO(p) & INGROUPMASK;
    int port = (g && g->filters[eltno] ? g->filters[eltno] - 1 : _default_output);
    checked_output_push(port, p);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AggregateFilter)
CLICK_ENDDECLS
