// -*- c-basic-offset: 2; related-file-name: "../../include/click/standard/scheduleinfo.hh" -*-
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
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>

ScheduleInfo::ScheduleInfo()
{
  MOD_INC_USE_COUNT;
#ifdef HAVE_STRIDE_SCHED
  static_assert((1 << FRAC_BITS) == Task::DEFAULT_TICKETS);
#endif
}

ScheduleInfo::~ScheduleInfo()
{
  MOD_DEC_USE_COUNT;
}

int
ScheduleInfo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  // find _prefix, which includes slash
  int last_slash = id().find_right('/');
  if (last_slash >= 0)
    _prefix = id().substring(0, last_slash + 1);
  else
    _prefix = String();

  // check for an earlier ScheduleInfo with the same prefix
  if (void *a = router()->attachment("ScheduleInfo." + _prefix))
    return ((ScheduleInfo *)a)->configure(conf, errh);
  router()->set_attachment("ScheduleInfo." + _prefix, this);
  
  // compile scheduling info
  for (int i = 0; i < conf.size(); i++) {
    Vector<String> parts;
    int mt;
    cp_spacevec(conf[i], parts);
    if (parts.size() == 0)
      /* empty argument OK */;
    else if (parts.size() != 2 || !cp_real2(parts[1], FRAC_BITS, &mt))
      errh->error("expected `ELEMENTNAME PARAM', got `%s'", String(conf[i]).cc());
    else {
      for (int j = 0; j < _element_names.size(); j++)
	if (_element_names[j] == parts[0]) {
	  if (_tickets[j] != mt)
	    errh->error("conflicting ScheduleInfo for `%s'", parts[0].cc());
	  goto appended;
	}
      _element_names.push_back(parts[0]);
      _tickets.push_back(mt);
     appended: ;
    }
  }
  
  return 0;
}

bool
ScheduleInfo::query(const String &text, int &tickets) const
{
  for (int i = 0; i < _element_names.size(); i++)
    if (_element_names[i] == text) {
      tickets = _tickets[i];
      return true;
    }
  return false;
}

bool
ScheduleInfo::query_prefixes(const String &id, int &scaled_tickets,
			     String &output_id) const
{
  // Check `id', then `id's prefix, then its prefix, and so forth.
  int tickets;
  String text = id;
  while (text) {
    if (query(text, tickets)) {
      scaled_tickets =
	((long long)scaled_tickets * tickets) >> FRAC_BITS;
      output_id = id.substring(text.length() + 1);
      return true;
    }
    int slash = text.find_right('/');
    text = text.substring(0, (slash < 0 ? 0 : slash));
  }
  return false;
}

int
ScheduleInfo::query(Element *e, ErrorHandler *errh)
{
#ifdef HAVE_STRIDE_SCHED
  // check prefixes in order of increasing length
  Router *r = e->router();
  String id = e->id();
  String text = id;
  int tickets = Task::DEFAULT_TICKETS;
  int slash;
  while (text) {

    // find current prefix, which includes slash
    String prefix = id.substring(0, id.length() - text.length());
    bool warning = false;	// only warn if there was partial ScheduleInfo

    if (void *a = r->attachment("ScheduleInfo." + prefix)) {
      if (((ScheduleInfo *)a)->query_prefixes(text, tickets, text))
	goto found;
      warning = true;
    }

    slash = text.find_left('/');
    if (slash >= 0) {
      if (warning)
	errh->warning("no ScheduleInfo for compound element `%s'",
		      text.substring(0, slash).cc());
      text = text.substring(slash + 1);
    } else {
      if (warning)
	errh->warning("no ScheduleInfo for element `%s'", text.cc());
      text = String();
    }
    
   found: ;
  }

  // check for too many tickets
  if (tickets > Task::MAX_TICKETS) {
    tickets = Task::MAX_TICKETS;
    String m = cp_unparse_real2(tickets, FRAC_BITS);
    errh->warning("ScheduleInfo too high; reduced to %s", m.cc());
  }
  
  // return the result you've got
  return tickets;
#else
  return 1;
#endif
}

void
ScheduleInfo::initialize_task(Element *e, Task *task, bool schedule,
			      ErrorHandler *errh)
{
#ifdef HAVE_STRIDE_SCHED
  int tickets = query(e, errh);
  if (tickets > 0) {
    task->initialize(e, schedule);
    task->set_tickets(tickets);
  }
#else
  task->initialize(e, schedule);
#endif
}

EXPORT_ELEMENT(ScheduleInfo)
ELEMENT_HEADER(<click/standard/scheduleinfo.hh>)
