/*
 * SRScheduler.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussrschedulers Institute of Technology
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <elements/grid/linktable.hh>
#include <elements/grid/arptable.hh>
#include <elements/grid/sr/path.hh>
#include <elements/grid/sr/srcrstat.hh>
#include "srscheduler.hh"
#include "srforwarder.hh"
#include "srpacket.hh"
#include <elements/standard/pullswitch.hh>
#include <click/router.hh>
CLICK_DECLS

#ifndef srscheduler_assert
#define srscheduler_assert(e) ((e) ? (void) 0 : srscheduler_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


SRScheduler::SRScheduler()
  :  Element(2,2),
     _ps(0)
{
  MOD_INC_USE_COUNT;
}

SRScheduler::~SRScheduler()
{
  MOD_DEC_USE_COUNT;
}

int
SRScheduler::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  unsigned int duration_ms = 0;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "DURATION", cpUnsigned, "ms", &duration_ms,
		    "PS", cpElement, "Pull Switch", &_ps,
                    0);

  if (!duration_ms) 
    return errh->error("DURATION not specified");
  if (!_ps) 
    return errh->error("PullSwitch PS must be specified");


  timerclear(&_duration);
  /* convert path_duration from ms to a struct timeval */
  _duration.tv_sec = duration_ms/1000;
  _duration.tv_usec = (duration_ms % 1000) * 1000;
  
  return ret;
}

SRScheduler *
SRScheduler::clone () const
{
  return new SRScheduler;
}

int
SRScheduler::initialize (ErrorHandler *)
{
  return 0;
}

void
SRScheduler::push(Packet *p_in , int port)
{
  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  
  if (!pk->flag(FLAG_SCHEDULE)) {
    click_chatter("SRScheduler %s: schedule flag not set",
		  id().cc());
    output(port).push(p_in);
    return;
  }

  Path p;
  for (int x = 0; x < pk->num_hops(); x++) {
    p.push_back(pk->get_hop(x));
  }


  ScheduleInfo *nfo = _schedules.findp(p);

  if (!nfo) {
    _schedules.insert(p, ScheduleInfo());
    nfo = _schedules.findp(p);
    nfo->_scheduled = false;
    nfo->_p = p;
  }

  click_gettimeofday(&nfo->_last_rx);

  nfo->_source = (port == 1);
  
  if (!nfo->_scheduled) {
    if (nfo->_source) {
      timeradd(&_duration, &nfo->_last_rx, &nfo->_next_start);
    } else {
      nfo->_next_start = nfo->_last_rx;
    }
    Timer *t = new Timer(static_start_hook, (void *) this);
    t->initialize(this);
    t->schedule_at(nfo->_next_start);

    nfo->_scheduled = true;
  }
  output(port).push(p_in);
  return;
}

void 
SRScheduler::start_hook()
{
  struct timeval now;
  click_gettimeofday(&now);
  Vector<Path> to_schedule;
  for (STIter iter = _schedules.begin(); iter; iter++) {
    ScheduleInfo nfo = iter.value();
    if (nfo._scheduled && timercmp(&nfo._next_start, &now, <)) {
      to_schedule.push_back(nfo._p);
    }
  }

  for (int x = 0; x < to_schedule.size(); x++) {
    start_path(to_schedule[x]);
  }

}

void 
SRScheduler::end_hook()
{
  struct timeval now;
  click_gettimeofday(&now);
  Vector<Path> to_end;
  for (STIter iter = _schedules.begin(); iter; iter++) {
    ScheduleInfo nfo = iter.value();
    if (nfo._scheduled && timercmp(&nfo._next_end, &now, <)) {
      to_end.push_back(nfo._p);
    }
  }

  for (int x = 0; x < to_end.size(); x++) {
    end_path(to_end[x]);
  }

}


void 
SRScheduler::call_switch(int i) {
  ErrorHandler *errh = ErrorHandler::default_handler();
  
  StringAccum s;
  s << i;
  const Router::Handler *h = Router::handler(_ps, String("switch"));
  if (!h) {
    errh->error("%s: no handler `%s'", id().cc(), 
		Router::Handler::unparse_name(_ps, String("switch")).cc());
  }
  
  if (h->writable()) {
    ContextErrorHandler cerrh
      (errh, "In write handler `" + h->unparse_name(_ps) + "':");
    h->call_write(s.take_string(), (Element *)_ps, &cerrh);
  } else {
    errh->error("%s: no write handler `%s'", 
		id().cc(), 
		h->unparse_name(_ps).cc());
  }

}

void
SRScheduler::start_path(Path p) {
  ScheduleInfo *nfo = _schedules.findp(p);
  if (!nfo) {
    click_chatter("SRScheduler %s: couldn't find info for %s\n",
		  id().cc(),
		  path_to_string(p).cc());
    return;
  }

  StringAccum sa;
  struct timeval now;
  click_gettimeofday(&now);
  sa << "Schedule " << id().cc() << ": now " << now << " vs "
     << nfo->_next_start << " start_path " << path_to_string(nfo->_p) << "\n";
  click_chatter("%s", sa.take_string().cc());


  timeradd(&_duration, &nfo->_next_start, &nfo->_next_end);
  Timer *t = new Timer(static_end_hook, (void *) this);
  t->initialize(this);
  t->schedule_at(nfo->_next_end);

  call_switch(0);
}


void
SRScheduler::end_path(Path p) {
  ScheduleInfo *nfo = _schedules.findp(p);
  if (!nfo) {
    click_chatter("SRScheduler %s: couldn't find info for %s\n",
		  id().cc(),
		  path_to_string(p).cc());
    return;
  }

  StringAccum sa;
  struct timeval now;
  click_gettimeofday(&now);
  sa << "Schedule " << id().cc() << ": now " << now << " vs "
     << nfo->_next_start << " end_path " << path_to_string(nfo->_p) << "\n";
  click_chatter("%s", sa.take_string().cc());


  for( int x = 0; x < p.size() - 1; x++) {
    struct timeval tmp = nfo->_next_start;
    timeradd(&tmp, &_duration, &nfo->_next_start);
  }
  Timer *t = new Timer(static_start_hook, (void *) this);
  t->initialize(this);
  t->schedule_at(nfo->_next_start);

  call_switch(-1);
}

int
SRScheduler::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  SRScheduler *n = (SRScheduler *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}
void
SRScheduler::clear() 
{

}

void
SRScheduler::add_handlers()
{
  add_write_handler("clear", static_clear, 0);
}

void
SRScheduler::srscheduler_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("SRScheduler %s assertion \"%s\" failed: file %s, line %d",
		id().cc(), expr, file, line);
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif

}

// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<IPAddress, Path>;
template class BigHashMap<Path, ScheduleInfo>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRScheduler)
