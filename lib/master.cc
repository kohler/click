// -*- c-basic-offset: 4; related-file-name: "../include/click/master.hh" -*-
/*
 * master.{cc,hh} -- Click event master
 * Eddie Kohler
 *
 * Copyright (c) 2003-7 The Regents of the University of California
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
#include <click/master.hh>
#include <click/element.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/handlercall.hh>
#if CLICK_USERLEVEL
# include <click/userutils.hh>
#endif
#if CLICK_USERLEVEL && HAVE_SYS_EVENT_H && HAVE_KQUEUE
# include <sys/event.h>
# if HAVE_EV_SET_UDATA_POINTER
#  define EV_SET_UDATA_CAST	(void *)
# else
#  define EV_SET_UDATA_CAST	/* nothing */
# endif
#endif
CLICK_DECLS

#if CLICK_USERLEVEL && (!HAVE_POLL_H || HAVE_USE_SELECT)
enum { POLLIN = Element::SELECT_READ, POLLOUT = Element::SELECT_WRITE };
#endif

#if CLICK_USERLEVEL
volatile sig_atomic_t Master::signals_pending;
static volatile sig_atomic_t signal_pending[NSIG];
#endif

Master::Master(int nthreads)
    : _routers(0), _pending_head(0), _pending_tail(&_pending_head)
{
    _refcount = 0;
    _stopper = 0;
    _master_paused = 0;

    for (int tid = -2; tid < nthreads; tid++)
	_threads.push_back(new RouterThread(this, tid));

    // timer information
#if CLICK_NS
    _max_timer_stride = 1;
#else
    _max_timer_stride = 32;
#endif
    _timer_stride = _max_timer_stride;
    _timer_count = 0;
#if CLICK_LINUXMODULE
    _timer_check_reports = 5;
#else
    _timer_check_reports = 0;
#endif

#if CLICK_USERLEVEL
    // select information
# if HAVE_USE_KQUEUE
    _kqueue = kqueue();
    _selected_callno = 0;
# endif
# if !HAVE_POLL_H || HAVE_USE_SELECT
    FD_ZERO(&_read_select_fd_set);
    FD_ZERO(&_write_select_fd_set);
    _max_select_fd = -1;
# endif
    assert(!_pollfds.size() && !_element_selectors.size());
    // Add a null 'struct pollfd', then take it off. This ensures that
    // _pollfds.begin() is nonnull, preventing crashes on Mac OS X
    struct pollfd dummy;
    dummy.events = dummy.fd = 0;
# if HAVE_POLL_H && !HAVE_USE_SELECT
    dummy.revents = 0;
# endif
    _pollfds.push_back(dummy);
    _pollfds.clear();
# if HAVE_MULTITHREAD
    _selecting_processor = click_invalid_processor();
# endif

    // signal information
    signals_pending = 0;
    _siginfo = 0;
    _signal_adding = false;
#endif

#if CLICK_LINUXMODULE
    spin_lock_init(&_master_lock);
    _master_lock_task = 0;
    _master_lock_count = 0;
    spin_lock_init(&_timer_lock);
    _timer_task = 0;
#endif
    _timer_check = Timestamp::now();
    _timer_check_reports = 0;

#if CLICK_NS
    _simnode = 0;
#endif
}

Master::~Master()
{
    lock_master();
    _refcount++;
    while (_routers) {
	Router *r = _routers;
	r->use();
	unlock_master();
	unregister_router(r);
	r->unuse();
	lock_master();
    }
    _refcount--;
    unlock_master();

    if (_refcount > 0)
	click_chatter("deleting master while ref count = %d", _refcount);

    for (int i = 0; i < _threads.size(); i++)
	delete _threads[i];
#if CLICK_USERLEVEL && HAVE_USE_KQUEUE
    if (_kqueue >= 0)
	close(_kqueue);
#endif
}

void
Master::use()
{
    lock_master();
    _refcount++;
    unlock_master();
}

void
Master::unuse()
{
    lock_master();
    _refcount--;
    bool del = (_refcount <= 0);
    unlock_master();
    if (del)
	delete this;
}

void
Master::pause()
{
    lock_timers();
#if CLICK_USERLEVEL
    _select_lock.acquire();
#endif
    SpinlockIRQ::flags_t flags = _master_task_lock.acquire();
    _master_paused++;
    _master_task_lock.release(flags);
#if CLICK_USERLEVEL
    _select_lock.release();
#endif
    unlock_timers();
}


// ROUTERS

void
Master::register_router(Router *router)
{
    lock_master();
    assert(router && router->_master == 0 && router->_running == Router::RUNNING_INACTIVE && !router->_next_router);
    _refcount++;		// balanced in unregister_router()
    router->_master = this;
    router->_next_router = _routers;
    _routers = router;
    unlock_master();
}

void
Master::prepare_router(Router *router)
{
    // increments _master_paused; should quickly call run_router() or
    // kill_router()
    lock_master();
    assert(router && router->_master == this && router->_running == Router::RUNNING_INACTIVE);
    router->_running = Router::RUNNING_PREPARING;
    unlock_master();
    pause();
}

void
Master::run_router(Router *router, bool foreground)
{
    lock_master();
    assert(router && router->_master == this && router->_running == Router::RUNNING_PREPARING);
    router->_running = (foreground ? Router::RUNNING_ACTIVE : Router::RUNNING_BACKGROUND);
    unlock_master();
    unpause();
}

void
Master::kill_router(Router *router)
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif

    lock_master();
    assert(router && router->_master == this);
    int was_running = router->_running;
    router->_running = Router::RUNNING_DEAD;
    if (was_running >= Router::RUNNING_BACKGROUND)
	pause();
    else if (was_running == Router::RUNNING_PREPARING)
	/* nada */;
    else {
	/* could not have anything on the list */
	assert(was_running == Router::RUNNING_INACTIVE || was_running == Router::RUNNING_DEAD);
	unlock_master();
	return;
    }

    // Fix stopper
    _stopper = 1;
#if CLICK_LINUXMODULE && HAVE_LINUXMODULE_2_6
    preempt_disable();
#endif
    unlock_master();

    // Remove tasks
    for (RouterThread **tp = _threads.begin(); tp < _threads.end(); tp++)
	(*tp)->unschedule_router_tasks(router);

    // 4.Sep.2007 - Don't bother to remove pending tasks.  They will be
    // removed shortly anyway, either when the task itself is deleted or (more
    // likely) when the pending list is processed.

    // Remove timers
    {
	lock_timers();
	assert(!_timer_runchunk.size());
	Timer* t;
	for (Timer** tp = _timer_heap.end(); tp > _timer_heap.begin(); )
	    if ((t = *--tp, t->router() == router)) {
		remove_heap(_timer_heap.begin(), _timer_heap.end(), tp, timer_less(), timer_place(_timer_heap.begin()));
		_timer_heap.pop_back();
		t->_owner = 0;
		t->_schedpos1 = 0;
	    }
	set_timer_expiry();
	unlock_timers();
    }

#if CLICK_USERLEVEL
    // Remove selects
    _select_lock.acquire();
    for (int pi = 0; pi < _pollfds.size(); pi++) {
	int fd = _pollfds[pi].fd;
	// take components out of the arrays early
	if (fd < _element_selectors.size()) {
	    ElementSelector &es = _element_selectors.at_u(fd);
	    if (es.read && es.read->router() == router)
		remove_pollfd(pi, POLLIN);
	    if (es.write && es.write->router() == router)
		remove_pollfd(pi, POLLOUT);
	}
	if (pi < _pollfds.size() && _pollfds[pi].fd != fd)
	    pi--;
    }
    _select_lock.release();

    // Remove signals
    {
	_signal_lock.acquire();
	SignalInfo **pprev = &_siginfo;
	for (SignalInfo *si = *pprev; si; si = *pprev)
	    if (si->router == router) {
		remove_signal_handler(si->signo, si->router, si->handler);
		pprev = &_siginfo;
	    } else
		pprev = &si->next;
	_signal_lock.release();
    }
#endif

    unpause();
#if CLICK_LINUXMODULE && HAVE_LINUXMODULE_2_6
    preempt_enable_no_resched();
#endif

    // something has happened, so wake up threads
    for (RouterThread** tp = _threads.begin() + 2; tp < _threads.end(); tp++)
	(*tp)->wake();
}

void
Master::unregister_router(Router *router)
{
    assert(router);
    lock_master();

    if (router->_master) {
	assert(router->_master == this);

	if (router->_running >= Router::RUNNING_PREPARING)
	    kill_router(router);

	Router **pprev = &_routers;
	for (Router *r = *pprev; r; r = r->_next_router)
	    if (r != router) {
		*pprev = r;
		pprev = &r->_next_router;
	    }
	*pprev = 0;
	_refcount--;		// balanced in register_router()
	router->_master = 0;
    }

    unlock_master();
}

bool
Master::check_driver()
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif

    lock_master();
    _stopper = 0;
    bool any_active = false;

    for (Router *r = _routers; r; ) {
	Router *next_router = r->_next_router;
	if (r->runcount() <= 0 && r->_running >= Router::RUNNING_BACKGROUND) {
	    Element *dm = (Element *)(r->attachment("Script"));
	    if (dm) {
		int max = 1000;
		while (HandlerCall::call_write(dm, "step", "router", ErrorHandler::default_handler()) == 0
		       && r->runcount() <= 0 && --max >= 0)
		    /* do nothing */;
	    }
	    if (r->runcount() <= 0 && r->_running >= Router::RUNNING_BACKGROUND) {
		kill_router(r);
		goto next;
	    }
	}
	if (r->_running == Router::RUNNING_ACTIVE)
	    any_active = true;
    next:
	r = next_router;
    }

    if (!any_active)
	_stopper = 1;
    unlock_master();
    return any_active;
}


// PENDING TASKS

void
Master::process_pending(RouterThread *thread)
{
    // must be called with thread's lock acquired

    // claim the current pending list
    SpinlockIRQ::flags_t flags = _master_task_lock.acquire();
    if (_master_paused > 0) {
	_master_task_lock.release(flags);
	return;
    }
    uintptr_t my_pending = _pending_head;
    _pending_head = 0;
    _pending_tail = &_pending_head;
    thread->_any_pending = 0;
    _master_task_lock.release(flags);

    // process the list
    while (Task *t = Task::pending_to_task(my_pending)) {
	my_pending = t->_pending_nextptr;
	t->_pending_nextptr = 0;
	t->process_pending(thread);
    }
}


// TIMERS

void
Master::set_max_timer_stride(unsigned timer_stride)
{
    _max_timer_stride = timer_stride;
    if (_timer_stride > _max_timer_stride)
	_timer_stride = _max_timer_stride;
}

void
Master::check_timer_expiry(Timer *t)
{
    // do not schedule timers for too far in the past
    if (t->_expiry.sec() + Timer::behind_sec < _timer_check.sec()) {
	if (_timer_check_reports > 0) {
	    --_timer_check_reports;
	    click_chatter("timer %p outdated expiry %{timestamp} updated to %{timestamp}", t, &t->_expiry, &_timer_check, &t->_expiry);
	}
	t->_expiry = _timer_check;
    }
}

inline void
Master::run_one_timer(Timer *t)
{
#if CLICK_STATS >= 2
    click_cycles_t start_cycles = click_get_cycles();
#endif

    t->_hook.callback(t, t->_thunk);

#if CLICK_STATS >= 2
    t->_owner->_timer_cycles += click_get_cycles() - start_cycles;
    t->_owner->_timer_calls++;
#endif
}

void
Master::run_timers()
{
    if (!attempt_lock_timers())
	return;
    if (_master_paused == 0 && _timer_heap.size() > 0 && !_stopper) {
#if CLICK_LINUXMODULE
	_timer_task = current;
#endif
	_timer_check = Timestamp::now();
	Timer *t = _timer_heap.at_u(0);

	if (t->_expiry <= _timer_check) {
	    // potentially adjust timer stride
	    Timestamp adj_expiry = t->_expiry + Timer::adjustment();
	    if (adj_expiry <= _timer_check) {
		_timer_count = 0;
		if (_timer_stride > 1)
		    _timer_stride = (_timer_stride * 4) / 5;
	    } else if (++_timer_count >= 12) {
		_timer_count = 0;
		if (++_timer_stride >= _max_timer_stride)
		    _timer_stride = _max_timer_stride;
	    }

	    // actually run timers
	    int max_timers = 64;
	    do {
		pop_heap(_timer_heap.begin(), _timer_heap.end(), timer_less(), timer_place(_timer_heap.begin()));
		_timer_heap.pop_back();
		set_timer_expiry();
		t->_schedpos1 = 0;

		run_one_timer(t);
	    } while (_timer_heap.size() > 0 && !_stopper
		     && (t = _timer_heap.at_u(0), t->_expiry <= _timer_check)
		     && --max_timers >= 0);

	    // If we ran out of timers to run, then perhaps there's an
	    // infinite timer loop or one timer is very far behind system
	    // time.  Eventually the system would catch up and run all timers,
	    // but in the meantime other timers could starve.  We detect this
	    // case and run ALL expired timers, reducing possible damage.
	    if (max_timers < 0 && !_stopper) {
		_timer_runchunk.reserve(32);
		do {
		    pop_heap(_timer_heap.begin(), _timer_heap.end(), timer_less(), timer_place(_timer_heap.begin()));
		    _timer_heap.pop_back();
		    t->_schedpos1 = -_timer_runchunk.size() - 1;

		    _timer_runchunk.push_back(t);
		} while (_timer_heap.size() > 0
			 && (t = _timer_heap.at_u(0), t->_expiry <= _timer_check));
		set_timer_expiry();

		Vector<Timer*>::iterator i = _timer_runchunk.begin();
		for (; !_stopper && i != _timer_runchunk.end(); ++i)
		    if (*i) {
			(*i)->_schedpos1 = 0;
			run_one_timer(*i);
		    }

		// reschedule unrun timers if stopped early
		for (; i != _timer_runchunk.end(); ++i)
		    if (*i) {
			(*i)->_schedpos1 = 0;
			(*i)->schedule_at((*i)->_expiry);
		    }
		_timer_runchunk.clear();
	    }
	}

#if CLICK_LINUXMODULE
	_timer_task = 0;
#endif
    }
    unlock_timers();
}


// SELECT

#if CLICK_USERLEVEL

namespace {
enum { SELECT_READ = Element::SELECT_READ, SELECT_WRITE = Element::SELECT_WRITE };
}

int
Master::add_select(int fd, Element *element, int mask)
{
    if (fd < 0)
	return -1;
    if (mask == 0)
	return 0;
    assert(element && (mask & ~(SELECT_READ | SELECT_WRITE)) == 0);
    _select_lock.acquire();

    // check whether to add readability, writability, or both; it is an error
    // for more than one element to wait on the same fd for the same status
    bool add_read = false, add_write = false;
    if (mask & SELECT_READ) {
	if (fd >= _element_selectors.size() || !_element_selectors[fd].read)
	    add_read = true;
	else if (_element_selectors[fd].read != element) {
	unlock_and_return_error:
	    _select_lock.release();
	    return -1;
	}
    }
    if (mask & SELECT_WRITE) {
	if (fd >= _element_selectors.size() || !_element_selectors[fd].write)
	    add_write = true;
	else if (_element_selectors[fd].write != element)
	    goto unlock_and_return_error;
    }
    if (!add_read && !add_write) {
	_select_lock.release();
	return 0;
    }

    // add the pollfd
    if (fd >= _fd_to_pollfd.size())
	_fd_to_pollfd.resize(fd + 1, -1);
    if (_fd_to_pollfd[fd] < 0) {
	_fd_to_pollfd[fd] = _pollfds.size();
	_pollfds.push_back(pollfd());
	_pollfds.back().fd = fd;
	_pollfds.back().events = 0;
    }
    int pi = _fd_to_pollfd[fd];

    // add the elements
    if (add_read) {
	if (fd >= _element_selectors.size())
	    _element_selectors.resize(fd + 1);
	_element_selectors[fd].read = element;
	_pollfds[pi].events |= POLLIN;
    }
    if (add_write) {
	if (fd >= _element_selectors.size())
	    _element_selectors.resize(fd + 1);
	_element_selectors[fd].write = element;
	_pollfds[pi].events |= POLLOUT;
    }

#if HAVE_USE_KQUEUE
    if (_kqueue >= 0) {
	// Add events to the kqueue
	struct kevent kev[2];
	int nkev = 0;
	if (add_read) {
	    EV_SET(&kev[nkev], fd, EVFILT_READ, EV_ADD, 0, 0, EV_SET_UDATA_CAST ((intptr_t) 0));
	    nkev++;
	}
	if (add_write) {
	    EV_SET(&kev[nkev], fd, EVFILT_WRITE, EV_ADD, 0, 0, EV_SET_UDATA_CAST ((intptr_t) 0));
	    nkev++;
	}
	int r = kevent(_kqueue, &kev[0], nkev, 0, 0, 0);
	if (r < 0) {
	    // Not all file descriptors are kqueueable.  So if we encounter
	    // a problem, fall back to select() or poll().
	    close(_kqueue);
	    _kqueue = -1;
	}
    }
#endif

#if !HAVE_POLL_H || HAVE_USE_SELECT
    // Add 'mask' to the fd_sets
    if (fd < FD_SETSIZE) {
	if (add_read)
	    FD_SET(fd, &_read_select_fd_set);
	if (add_write)
	    FD_SET(fd, &_write_select_fd_set);
	if (fd > _max_select_fd)
	    _max_select_fd = fd;
    } else {
	static int warned = 0;
# if HAVE_USE_KQUEUE
	if (_kqueue < 0)
# endif
	    if (!warned) {
		click_chatter("Master::add_select(%d): fd >= FD_SETSIZE", fd);
		warned = 1;
	    }
    }
#endif


#if HAVE_MULTITHREAD
    // need to wake up selecting thread since there's more to select
    if (_selecting_processor != click_invalid_processor())
	pthread_kill(_selecting_processor, SIGIO);
#endif

    _select_lock.release();
    return 0;
}

void
Master::remove_pollfd(int pi, int event)
{
    assert(event == POLLIN || event == POLLOUT);

    // remove event
    int fd = _pollfds[pi].fd;
    _pollfds[pi].events &= ~event;
    if (event == POLLIN)
	_element_selectors[fd].read = 0;
    else
	_element_selectors[fd].write = 0;

#if HAVE_USE_KQUEUE
    // remove event from kqueue
    if (_kqueue >= 0) {
	struct kevent kev;
	EV_SET(&kev, fd, (event == POLLIN ? EVFILT_READ : EVFILT_WRITE), EV_DELETE, 0, 0, EV_SET_UDATA_CAST ((intptr_t) 0));
	int r = kevent(_kqueue, &kev, 1, 0, 0, 0);
	if (r < 0)
	    click_chatter("Master::remove_pollfd(fd %d): kevent: %s", _pollfds[pi].fd, strerror(errno));
    }
#endif
#if !HAVE_POLL_H || HAVE_USE_SELECT
    // remove event from select list
    if (fd < FD_SETSIZE) {
	fd_set *fd_ptr = (event == POLLIN ? &_read_select_fd_set : &_write_select_fd_set);
	FD_CLR(fd, fd_ptr);
    }
#endif

    // exit unless there are no events left
    if (_pollfds[pi].events)
	return;

    // remove whole pollfd
    _pollfds[pi] = _pollfds.back();
    _pollfds.pop_back();
    _fd_to_pollfd[fd] = -1;
    if (pi < _pollfds.size())
	_fd_to_pollfd[_pollfds[pi].fd] = pi;
#if !HAVE_POLL_H || HAVE_USE_SELECT
    if (fd == _max_select_fd) {
	_max_select_fd = -1;
	for (int pix = 0; pix < _pollfds.size(); ++pix)
	    if (_pollfds[pix].fd < FD_SETSIZE
		&& _pollfds[pix].fd > _max_select_fd)
		_max_select_fd = _pollfds[pix].fd;
    }
#endif
}

int
Master::remove_select(int fd, Element *element, int mask)
{
    if (fd < 0)
	return -1;
    assert(element && (mask & ~(SELECT_READ | SELECT_WRITE)) == 0);
    _select_lock.acquire();

    bool remove_read = false, remove_write = false;
    if ((mask & SELECT_READ) && fd < _element_selectors.size()
	&& _element_selectors[fd].read == element)
	remove_read = true;
    if ((mask & SELECT_WRITE) && fd < _element_selectors.size()
	&& _element_selectors[fd].write == element)
	remove_write = true;
    if (!remove_read && !remove_write) {
	_select_lock.release();
	return -1;
    }

    int pi = _fd_to_pollfd[fd];
    if (remove_read)
	remove_pollfd(pi, POLLIN);
    if (remove_write)
	remove_pollfd(pi, POLLOUT);
    _select_lock.release();
    return 0;
}


#if HAVE_USE_KQUEUE
void
Master::run_selects_kqueue(bool more_tasks)
{
    // Decide how long to wait.
# if CLICK_NS
    // Never block if we're running in the simulator.
    struct timespec wait, *wait_ptr = &wait;
    wait.tv_sec = wait.tv_nsec = 0;
    (void) more_tasks;
# else /* !CLICK_NS */
    // Never wait if anything is scheduled; otherwise, if no timers, block
    // indefinitely.
    struct timespec wait, *wait_ptr = &wait;
    wait.tv_sec = wait.tv_nsec = 0;
    if (!more_tasks) {
	Timestamp t = next_timer_expiry_adjusted();
	if (t.sec() == 0)
	    wait_ptr = 0;
	else if (unlikely(Timestamp::warp_jumping()))
	    Timestamp::warp_jump(t);
	else if ((t -= Timestamp::now(), t.sec() >= 0))
	    wait = t.warp_real_delay().timespec();
    }
# endif

    // Bump selected_callno
    _selected_callno++;
    if (_selected_callno == 0) { // be anal about wraparound
	for (ElementSelector *es = _element_selectors.begin(); es != _element_selectors.end(); ++es)
	    es->callno = 0;
	_selected_callno++;
    }

# if HAVE_MULTITHREAD
    _selecting_processor = click_current_processor();
    _select_lock.release();
# endif

    struct kevent kev[64];
    int n = kevent(_kqueue, 0, 0, &kev[0], 64, wait_ptr);
    int was_errno = errno;
    run_signals();

# if HAVE_MULTITHREAD
    _select_lock.acquire();
    _selecting_processor = click_invalid_processor();
# endif

    if (n < 0 && was_errno != EINTR)
	perror("kevent");
    else if (n > 0)
	for (struct kevent *p = &kev[0]; p < &kev[n]; p++) {
	    int fd = (int) p->ident;
	    if (fd < _element_selectors.size()
		&& (p->filter == EVFILT_READ || p->filter == EVFILT_WRITE)) {
		ElementSelector &es = _element_selectors[fd];
		Element *e = (p->filter == EVFILT_READ ? es.read : es.write);
		if (e && (es.read != es.write
			  || es.callno != _selected_callno)) {
		    es.callno = _selected_callno;
		    e->selected(fd);
		}
	    }
	}
}
#endif /* HAVE_USE_KQUEUE */

#if HAVE_POLL_H && !HAVE_USE_SELECT
void
Master::run_selects_poll(bool more_tasks)
{
    // Decide how long to wait.
# if CLICK_NS
    // Never block if we're running in the simulator.
    int timeout = -1;
    (void) more_tasks;
# else
    // Never wait if anything is scheduled; otherwise, if no timers, block
    // indefinitely.
    int timeout = 0;
    if (!more_tasks) {
	Timestamp t = next_timer_expiry_adjusted();
	if (t.sec() == 0)
	    timeout = -1;
	else if (unlikely(Timestamp::warp_jumping()))
	    Timestamp::warp_jump(t);
	else if ((t -= Timestamp::now(), t.sec() >= 0)) {
	    t = t.warp_real_delay();
	    if (t.sec() >= INT_MAX / 1000)
		timeout = INT_MAX - 1000;
	    else
		timeout = t.msecval();
	}
    }
# endif /* CLICK_NS */

# if HAVE_MULTITHREAD
    // Need a private copy of _pollfds, since other threads may run while we
    // block
    Vector<struct pollfd> my_pollfds(_pollfds);
    _selecting_processor = click_current_processor();
    _select_lock.release();
# else
    Vector<struct pollfd> &my_pollfds(_pollfds);
# endif

    int n = poll(my_pollfds.begin(), my_pollfds.size(), timeout);
    int was_errno = errno;
    run_signals();

# if HAVE_MULTITHREAD
    _select_lock.acquire();
    _selecting_processor = click_invalid_processor();
# endif

    if (n < 0 && was_errno != EINTR)
	perror("poll");
    else if (n > 0)
	for (struct pollfd *p = my_pollfds.begin(); p < my_pollfds.end(); p++)
	    if (p->revents) {
		int pi = p - my_pollfds.begin();

		// Beware: calling 'selected()' might call remove_select(),
		// causing disaster! Load everything we need out of the
		// vectors before calling out.

		int fd = p->fd;
		Element *read_elt = 0, *write_elt;
		if ((p->revents & ~POLLOUT)
		    && (read_elt = _element_selectors[fd].read))
		    read_elt->selected(fd);
		if ((p->revents & ~POLLIN)
		    && (write_elt = _element_selectors[fd].write)
		    && read_elt != write_elt)
		    write_elt->selected(fd);

		// 31.Oct.2003 - Peter Swain: _pollfds may have grown or
		// shrunk!
		p = my_pollfds.begin() + pi;
		if (p < my_pollfds.end() && fd != p->fd)
		    p--;
	    }
}

#else /* !HAVE_POLL_H || HAVE_USE_SELECT */
void
Master::run_selects_select(bool more_tasks)
{
    // Decide how long to wait.
# if CLICK_NS
    // Never block if we're running in the simulator.
    struct timeval wait, *wait_ptr = &wait;
    timerclear(&wait);
    (void) more_tasks;
# else /* !CLICK_NS */
    // Never wait if anything is scheduled; otherwise, if no timers, block
    // indefinitely.
    struct timeval wait, *wait_ptr = &wait;
    timerclear(&wait);
    if (!more_tasks) {
	Timestamp t = next_timer_expiry_adjusted();
	if (t.sec() == 0)
	    wait_ptr = 0;
	else if (unlikely(Timestamp::warp_jumping()))
	    Timestamp::warp_jump(t);
	else if ((t -= Timestamp::now(), t.sec() >= 0))
	    wait = t.warp_real_delay().timeval();
    }
# endif /* CLICK_NS */

    fd_set read_mask = _read_select_fd_set;
    fd_set write_mask = _write_select_fd_set;

# if HAVE_MULTITHREAD
    _selecting_processor = click_current_processor();
    _select_lock.release();
# endif

    int n = select(_max_select_fd + 1, &read_mask, &write_mask, (fd_set*) 0, wait_ptr);
    int was_errno = errno;
    run_signals();

# if HAVE_MULTITHREAD
    _selecting_processor = click_invalid_processor();
    _select_lock.acquire();
# endif

    if (n < 0 && was_errno != EINTR)
	perror("select");
    else if (n > 0)
	for (struct pollfd *p = _pollfds.begin(); p < _pollfds.end(); p++)
	    if (p->fd >= FD_SETSIZE || FD_ISSET(p->fd, &read_mask)
		|| FD_ISSET(p->fd, &write_mask)) {
		int pi = p - _pollfds.begin();

		// Beware: calling 'selected()' might call remove_select(),
		// causing disaster! Load everything we need out of the
		// vectors before calling out.

		int fd = p->fd;
		Element *read_elt = 0, *write_elt;
		if ((fd >= FD_SETSIZE || FD_ISSET(fd, &read_mask))
		    && (read_elt = _element_selectors[fd].read))
		    read_elt->selected(fd);
		if ((fd >= FD_SETSIZE || FD_ISSET(fd, &write_mask))
		    && (write_elt = _element_selectors[fd].write)
		    && read_elt != write_elt)
		    write_elt->selected(fd);

		// 31.Oct.2003 - Peter Swain: _pollfds may have grown or
		// shrunk!
		p = _pollfds.begin() + pi;
		if (p < _pollfds.end() && fd != p->fd)
		    p--;
	    }
}
#endif /* HAVE_POLL_H && !HAVE_USE_SELECT */

void
Master::run_selects(bool more_tasks)
{
    // Wait in select() for input or timer, and call relevant elements'
    // selected() methods.

    if (!_select_lock.attempt())
	return;

#if HAVE_MULTITHREAD
    if (_selecting_processor != click_invalid_processor()) {
	// Another thread is blocked in select().  No point in this thread's
	// also checking file descriptors, so block if there are no more tasks
	// to run.  It *is* useful to wait until the next timer expiry,
	// because we may have set some new short-term timers while the other
	// thread was blocking.
	_select_lock.release();
	if (!more_tasks) {
	    struct timeval wait, *wait_ptr = &wait;
	    Timestamp t = next_timer_expiry_adjusted();
	    if (t.sec() == 0)
		wait_ptr = 0;
	    else if (unlikely(Timestamp::warp_jumping()))
		Timestamp::warp_jump(t);
	    else if ((t -= Timestamp::now(), t.sec() >= 0))
		wait = t.warp_real_delay().timeval();
	    ignore_result(select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0,
				 wait_ptr));
	}
	return;
    }
#endif

    // Return early if paused.
    if (_master_paused > 0)
	goto unlock_select_exit;

    // Return early if there are no selectors and there are tasks to run.
    if (_pollfds.size() == 0 && more_tasks)
	goto unlock_select_exit;

    // Call the relevant selector implementation.
#if HAVE_USE_KQUEUE
    if (_kqueue >= 0) {
	run_selects_kqueue(more_tasks);
	goto unlock_select_exit;
    }
#endif
#if HAVE_POLL_H && !HAVE_USE_SELECT
    run_selects_poll(more_tasks);
#else
    run_selects_select(more_tasks);
#endif

 unlock_select_exit:
    _select_lock.release();
}

#endif


// SIGNALS

#if CLICK_USERLEVEL

extern "C" {
static void
sighandler(int signo)
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, signo);
    sigprocmask(SIG_BLOCK, &sigset, 0);
#if !HAVE_SIGACTION
    signal(signo, SIG_DFL);
#endif
    Master::signals_pending = signal_pending[signo] = 1;
}
}

int
Master::add_signal_handler(int signo, Router *router, const String &handler)
{
    if (signo < 0 || signo >= 32 || router->master() != this)
	return -1;

    SignalInfo *si = new SignalInfo;
    if (!si)
	return -1;
    si->signo = signo;
    si->router = router;
    si->handler = handler;

    _signal_lock.acquire();
    _signal_adding = true;
    (void) remove_signal_handler(signo, router, handler);
    _signal_adding = false;

    si->next = _siginfo;
    _siginfo = si;

    click_signal(signo, sighandler, true);

    _signal_lock.release();
    return 0;
}

int
Master::remove_signal_handler(int signo, Router *router, const String &handler)
{
    _signal_lock.acquire();
    int nhandlers = 0, status = -1;
    SignalInfo **pprev = &_siginfo;
    for (SignalInfo *si = *pprev; si; si = *pprev)
	if (si->signo == signo && si->router == router
	    && si->handler == handler) {
	    *pprev = si->next;
	    delete si;
	    status = 0;
	} else {
	    if (si->signo == signo)
		nhandlers = 1;
	    pprev = &si->next;
	}

    if (!_signal_adding && status >= 0 && nhandlers == 0)
	click_signal(signo, SIG_DFL, false);
    _signal_lock.release();
    return status;
}

void
Master::process_signals()
{
    signals_pending = 0;
    char handled[NSIG];
    _signal_lock.acquire();

    SignalInfo *happened = 0;
    SignalInfo **pprev = &_siginfo;
    sigset_t sigset;
    sigemptyset(&sigset);
    for (SignalInfo *si = *pprev; si; si = *pprev)
	if (signal_pending[si->signo] && si->router->running()) {
	    *pprev = si->next;
	    si->next = happened;
	    happened = si;
	    handled[si->signo] = 0;
	    signal_pending[happened->signo] = 0;
	    sigaddset(&sigset, si->signo);
	} else
	    pprev = &si->next;

    SignalInfo *unhandled;
    SignalInfo **unhandled_pprev = &unhandled;
    while (happened) {
	SignalInfo *next = happened->next;
	if (HandlerCall::call_write(happened->handler, happened->router->root_element()) >= 0) {
	    handled[happened->signo] = 1;
	    delete happened;
	} else {
	    *unhandled_pprev = happened;
	    unhandled_pprev = &happened->next;
	}
	happened = next;
    }
    *unhandled_pprev = 0;

    sigprocmask(SIG_UNBLOCK, &sigset, 0);

    while (unhandled) {
	SignalInfo *next = unhandled->next;
	if (!handled[unhandled->signo])
	    kill(getpid(), unhandled->signo);
	delete unhandled;
	unhandled = next;
    }

    _signal_lock.release();
}

#endif


// NS

#if CLICK_NS

void
Master::initialize_ns(simclick_node_t *simnode)
{
    assert(!_simnode);
    _simnode = simnode;
}

#endif


#if CLICK_DEBUG_MASTER
#include <click/straccum.hh>

String
Master::info() const
{
    StringAccum sa;
    sa << "paused:\t" << _master_paused << '\n';
    sa << "stopper:\t" << _stopper << '\n';
    sa << "pending:\t" << (Task::pending_to_task(_pending_head) != 0) << '\n';
    for (int i = 0; i < _threads.size(); i++) {
	RouterThread *t = _threads[i];
	sa << "thread " << (i - 1) << ":";
# ifdef CLICK_LINUXMODULE
	if (t->_sleeper)
	    sa << "\tsleep";
	else
	    sa << "\twake";
# endif
	if (t->_pending)
	    sa << "\tpending";
	sa << '\n';
    }
    return sa.take_string();
}

#endif
CLICK_ENDDECLS
