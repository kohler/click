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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

AggregateFilter::Group::Group(uint32_t aggregate)
    : groupno(aggregate & GROUPMASK), next(0)
{
    memset(filters, 0, sizeof(filters));
}

AggregateFilter::AggregateFilter()
    : Element(1, 1), _default_output(1)
{
    MOD_INC_USE_COUNT;
    for (unsigned i = 0; i < NBUCKETS; i++)
	_groups[i] = 0;
}

AggregateFilter::~AggregateFilter()
{
    MOD_DEC_USE_COUNT;
}

void
AggregateFilter::notify_noutputs(int n)
{
    set_noutputs(n <= 254 ? n : 254);
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
    int before_nerrors = errh->nerrors();
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
	else if (words[0] == "deny")
	    port = 1;
	else if (cp_unsigned(words[0], (unsigned *)&port))
	    /* OK */;
	else if (words[0] != "drop") {
	    errh->error("pattern %d: expected a port number", argno);
	    continue;
	}

	if (words.size() == 1)
	    errh->error("pattern %d has no aggregates", argno);
	else if (words.size() == 2 && (words[1] == "all" || words[1] == "-"))
	    _default_output = port;
	else
	    for (int i = 1; i < words.size(); i++) {
		uint32_t agg1, agg2;
		int dash;
		if (cp_unsigned(words[i], &agg1))
		    agg2 = agg1;
		else if ((dash = words[i].find_left('-')) >= 0
			 && cp_unsigned(words[i].substring(0, dash), &agg1)
			 && cp_unsigned(words[i].substring(dash + 1), &agg2))
		    /* nada */;
		else {
		    errh->error("pattern %d: bad aggregate number `%#s'", words[i].cc());
		    continue;
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

    return (errh->nerrors() == before_nerrors ? 0 : -1);
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
    if (prev) {
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
