// -*- c-basic-offset: 4; related-file-name: "../../include/click/standard/scheduleinfo.hh" -*-
/*
 * scheduleinfo.{cc,hh} -- element stores schedule parameters
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/standard/scheduleinfo.hh>
#include <click/nameinfo.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/nameinfo.hh>
CLICK_DECLS

ScheduleInfo::ScheduleInfo()
{
#if HAVE_STRIDE_SCHED
    static_assert((1 << FRAC_BITS) == Task::DEFAULT_TICKETS, "Stride scheduler constant issue.");
#endif
}

int
ScheduleInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    NameDB* db = NameInfo::getdb(NameInfo::T_SCHEDULEINFO, this, 4, true);

    // compile scheduling info
    for (int i = 0; i < conf.size(); i++) {
	Vector<String> parts;
	uint32_t mt;
	cp_spacevec(conf[i], parts);
	if (parts.size() == 0)
	    /* empty argument OK */;
	else if (parts.size() != 2 || !FixedPointArg(FRAC_BITS).parse(parts[1], mt))
	    errh->error("expected %<ELEMENTNAME PARAM%>");
	else
	    db->define(parts[0], &mt, 4);
    }

    return 0;
}

int
ScheduleInfo::query(Element *e, ErrorHandler *errh)
{
#if HAVE_STRIDE_SCHED
    // check prefixes in order of increasing length
    String id = e->name();

    Vector<String> prefixes;
    prefixes.push_back(String());
    const char *slash = id.begin();
    while ((slash = find(slash, id.end(), '/')) < id.end()) {
	prefixes.push_back(id.substring(id.begin(), slash + 1));
	slash++;
    }
    prefixes.push_back(id);

    Vector<uint32_t> tickets(prefixes.size(), Task::DEFAULT_TICKETS);

    NameDB *db = NameInfo::getdb(NameInfo::T_SCHEDULEINFO, e, 4, false);
    while (db) {
	bool frobbed = false;
	for (int i = prefixes.size() - 1;
	     i >= 0 && prefixes[i].length() >= db->context().length();
	     i--)
	    if (db->query(prefixes[i].substring(db->context().length()), &tickets[i], 4))
		frobbed = true;
	    else if (frobbed)	// erase intermediate ticket settings
		tickets[i] = Task::DEFAULT_TICKETS;
	db = db->context_parent();
    }

    // multiply tickets
    int tickets_out = tickets[0];
    for (int i = 1; i < tickets.size(); i++)
	if (tickets[i] != Task::DEFAULT_TICKETS)
#ifdef HAVE_INT64_TYPES
	    tickets_out = ((int64_t) tickets_out * tickets[i]) >> FRAC_BITS;
#else
	    tickets_out = (tickets_out * tickets[i]) >> FRAC_BITS;
#endif

    // check for too many tickets
    if (tickets_out > Task::MAX_TICKETS) {
	tickets_out = Task::MAX_TICKETS;
	String m = cp_unparse_real2(tickets_out, FRAC_BITS);
	errh->warning("ScheduleInfo too high; reduced to %s", m.c_str());
    }

    // return the result you've got
    return tickets_out;
#else
    (void) e, (void) errh;
    return 1;
#endif
}

void
ScheduleInfo::initialize_task(Element *e, Task *task, bool schedule,
			      ErrorHandler *errh)
{
#if HAVE_STRIDE_SCHED
    int tickets = query(e, errh);
    if (tickets > 0) {
	task->initialize(e, schedule);
	task->set_tickets(tickets);
    }
#else
    (void) errh;
    task->initialize(e, schedule);
#endif
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ScheduleInfo)
ELEMENT_HEADER(<click/standard/scheduleinfo.hh>)
