// -*- c-basic-offset: 4 -*-
#include <click/config.h>
#include "staticthreadsched.hh"
#include <click/task.hh>
#include <click/master.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/confparse.hh>

StaticThreadSched::StaticThreadSched()
    : _next_thread_sched(0)
{
}

StaticThreadSched::~StaticThreadSched()
{
}

int
StaticThreadSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e;
    int preference;
    for (int i = 0; i < conf.size(); i++) {
	if (cp_va_space_parse(conf[i], this, errh,
			      cpElement, "element", &e,
			      cpInteger, "thread ID", &preference,
			      cpEnd) < 0)
	    return -1;
	if (e->eindex() >= _thread_preferences.size())
	    _thread_preferences.resize(e->eindex() + 1, THREAD_UNKNOWN);
	if (preference < -1 || preference >= master()->nthreads()) {
	    errh->warning("thread preference %d out of range", preference);
	    preference = (preference < 0 ? -1 : 0);
	}
	_thread_preferences[e->eindex()] = preference;
    }
    _next_thread_sched = router()->thread_sched();
    router()->set_thread_sched(this);
    return 0;
}

int
StaticThreadSched::initial_home_thread_id(Task *task, bool scheduled)
{
    if (Element *e = task->element()) {
	int eidx = e->eindex();
	if (eidx >= 0 && eidx < _thread_preferences.size() && _thread_preferences[eidx] != THREAD_UNKNOWN)
	    return _thread_preferences[eidx];
    }
    if (_next_thread_sched)
	return _next_thread_sched->initial_home_thread_id(task, scheduled);
    else
	return THREAD_UNKNOWN;
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(StaticThreadSched)
