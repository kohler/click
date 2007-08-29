// -*- c-basic-offset: 4; related-file-name: "../include/click/master.hh" -*-
/*
 * master.{cc,hh} -- Click event master
 * Eddie Kohler
 *
 * Copyright (c) 2003-7 The Regents of the University of California
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
#include <click/handlercall.hh>
#if CLICK_USERLEVEL && HAVE_SYS_EVENT_H && HAVE_KQUEUE
# include <sys/event.h>
# if HAVE_EV_SET_UDATA_POINTER
#  define EV_SET_UDATA_CAST	(void *)
# else
#  define EV_SET_UDATA_CAST	/* nothing */
# endif
#endif
#if CLICK_USERLEVEL
# include <signal.h>
#endif
CLICK_DECLS

#if CLICK_USERLEVEL && !HAVE_POLL_H
enum { POLLIN = Element::SELECT_READ, POLLOUT = Element::SELECT_WRITE };
#endif

#if CLICK_USERLEVEL
atomic_uint32_t Master::signals_pending;
#endif

Master::Master(int nthreads)
    : _routers(0), _current_pending(0)
{
    _refcount = 0;
    _stopper = 0;
    _master_paused = 0;

    for (int tid = -2; tid < nthreads; tid++)
	_threads.push_back(new RouterThread(this, tid));

    _pending_head[0] = _pending_head[1] = 0;
    _pending_tail[0] = &_pending_head[0];
    _pending_tail[1] = &_pending_head[1];
    
#if CLICK_USERLEVEL
# if HAVE_SYS_EVENT_H && HAVE_KQUEUE
    _kqueue = kqueue();
    _selected_callno = 0;
# endif
# if !HAVE_POLL_H
    FD_ZERO(&_read_select_fd_set);
    FD_ZERO(&_write_select_fd_set);
    _max_select_fd = -1;
# endif
    assert(!_pollfds.size() && !_read_poll_elements.size() && !_write_poll_elements.size());
    // Add a null 'struct pollfd', then take it off. This ensures that
    // _pollfds.begin() is nonnull, preventing crashes on Mac OS X
    struct pollfd dummy;
    dummy.events = dummy.fd = 0;
# if HAVE_POLL_H
    dummy.revents = 0;
# endif
    _pollfds.push_back(dummy);
    _pollfds.clear();

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
    
#if CLICK_NS
    _siminst = 0;
    _clickinst = 0;
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
#if CLICK_USERLEVEL && HAVE_SYS_EVENT_H && HAVE_KQUEUE
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
    SpinlockIRQ::flags_t flags = _task_lock.acquire();
    _master_paused++;
    _task_lock.release(flags);
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
    
    {
	SpinlockIRQ::flags_t flags = _task_lock.acquire();

	// Remove pending tasks
	for (unsigned x = 0; x < 2; x++) {
	    SpinlockIRQ::flags_t pflags = _pending_lock[x].acquire();
	    uintptr_t *tptr = &_pending_head[x];
	    while (Task *t = Task::pending_to_task(*tptr))
		if (t->_router == router) {
		    *tptr = t->_pending_nextptr;
		    t->_pending_nextptr = 0;
		    t->_pending_reschedule = 0;
		} else
		    tptr = &t->_pending_nextptr;
	    _pending_tail[x] = tptr;
	    _pending_lock[x].release(pflags);
	}

	_task_lock.release(flags);
    }
    
    // Remove timers
    {
	lock_timers();
	Timer* t;
	for (Timer** tp = _timer_heap.end(); tp > _timer_heap.begin(); )
	    if ((t = *--tp, t->_router == router)) {
		timer_reheapify_from(tp - _timer_heap.begin(), _timer_heap.back(), true);
		// must clear _schedpos AFTER timer_reheapify_from()
		t->_router = 0;
		t->_schedpos = -1;
		// recheck this slot; have moved a timer there
		_timer_heap.pop_back();
		if (tp < _timer_heap.end())
		    tp++;
	    }
	unlock_timers();
    }

#if CLICK_USERLEVEL
    // Remove selects
    _select_lock.acquire();
    for (int pi = 0; pi < _pollfds.size(); pi++) {
	int fd = _pollfds[pi].fd;
	// take components out of the arrays early
	Element* read_element = _read_poll_elements[pi];
	Element* write_element = _write_poll_elements[pi];
	if (read_element && read_element->router() == router)
	    remove_pollfd(pi, POLLIN);
	if (write_element && write_element->router() == router)
	    remove_pollfd(pi, POLLOUT);
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
		while (HandlerCall::call_write(dm, "step", "router") == 0
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
    SpinlockIRQ::flags_t flags = _task_lock.acquire();
    if (_master_paused > 0) {
	_task_lock.release(flags);
	return;
    }
    // switch lists
    unsigned cp = _current_pending;
    _current_pending = 1 - _current_pending;
    assert(_pending_head[_current_pending] == 0 && _pending_tail[_current_pending] == &_pending_head[_current_pending]);
    thread->_pending = 0;
    _task_lock.release(flags);

    // process the list
    flags = _pending_lock[cp].acquire();
    while (Task *t = Task::pending_to_task(_pending_head[cp])) {
	_pending_head[cp] = t->_pending_nextptr;
	t->_pending_nextptr = 0;
	t->process_pending(thread);
    }
    _pending_tail[cp] = &_pending_head[cp];
    _pending_lock[cp].release(flags);
}


// TIMERS

void
Master::timer_reheapify_from(int pos, Timer* t, bool will_delete)
{
    // MUST be called with timer lock held
    Timer** tbegin = _timer_heap.begin();
    Timer** tend = _timer_heap.end();
    int npos;

    while (pos > 0
	   && (npos = (pos-1) >> 1, tbegin[npos]->_expiry > t->_expiry)) {
	tbegin[pos] = tbegin[npos];
	tbegin[npos]->_schedpos = pos;
	pos = npos;
    }

    while (1) {
	Timer* smallest = t;
	Timer** tp = tbegin + 2*pos + 1;
	if (tp < tend && tp[0]->_expiry <= smallest->_expiry)
	    smallest = tp[0];
	if (tp + 1 < tend && tp[1]->_expiry <= smallest->_expiry)
	    smallest = tp[1], tp++;

	smallest->_schedpos = pos;
	tbegin[pos] = smallest;

	if (smallest == t)
	    break;

	pos = tp - tbegin;
    }

    if (tbegin + 1 < tend || !will_delete)
	_timer_expiry = tbegin[0]->_expiry;
    else
	_timer_expiry = Timestamp();
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
	Timestamp now = Timestamp::now();
	Timer* t;
	while (_timer_heap.size() > 0 && !_stopper
	       && (t = _timer_heap.at_u(0), t->_expiry <= now)) {
	    timer_reheapify_from(0, _timer_heap.back(), true);
	    // must reset _schedpos AFTER timer_reheapify_from
	    t->_schedpos = -1;
	    _timer_heap.pop_back();
	    t->_hook(t, t->_thunk);
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

    int pi = _pollfds.size();
    for (int pix = 0; pix < _pollfds.size(); pix++)
	if (_pollfds[pix].fd == fd) {
	    // There is exactly one match per fd.
	    if (((mask & SELECT_READ) && (_pollfds[pix].events & POLLIN) && _read_poll_elements[pix] != element)
		|| ((mask & SELECT_WRITE) && (_pollfds[pix].events & POLLOUT) && _write_poll_elements[pix] != element)) {
		_select_lock.release();
		return -1;
	    }
	    pi = pix;
	    break;
	} else if (_pollfds[pix].fd < 0)
	    pi = pix;

    // Add a new selector
    if (pi == _pollfds.size()) {
	_pollfds.push_back(pollfd());
	_pollfds[pi].events = 0;
	_read_poll_elements.push_back(0);
	_write_poll_elements.push_back(0);
#if HAVE_SYS_EVENT_H && HAVE_KQUEUE
	_selected_callnos.push_back(0);
#endif
    }
    _pollfds[pi].fd = fd;

    // Add selectors
    if (mask & SELECT_READ) {
	_pollfds[pi].events |= POLLIN;
	_read_poll_elements[pi] = element;
    }
    if (mask & SELECT_WRITE) {
	_pollfds[pi].events |= POLLOUT;
	_write_poll_elements[pi] = element;
    }

#if HAVE_SYS_EVENT_H && HAVE_KQUEUE
    if (_kqueue >= 0) {
	// Add events to the kqueue
	struct kevent kev[2];
	int nkev = 0;
	if (mask & SELECT_READ) {
	    EV_SET(&kev[nkev], fd, EVFILT_READ, EV_ADD, 0, 0, EV_SET_UDATA_CAST ((intptr_t) pi));
	    nkev++;
	}
	if (mask & SELECT_WRITE) {
	    EV_SET(&kev[nkev], fd, EVFILT_WRITE, EV_ADD, 0, 0, EV_SET_UDATA_CAST ((intptr_t) pi));
	    nkev++;
	}
	int r = kevent(_kqueue, &kev[0], nkev, 0, 0, 0);
	if (r < 0) {
	    // Not all file descriptors are kqueueable.  So if we encounter
	    // a problem, fall back to select() or poll().
	    // click_chatter("Master::add_select(%d, %d): kevent: %s", (int) kev[0].ident, kev[0].filter, strerror(errno));
	    close(_kqueue);
	    _kqueue = -1;
	    // Clean blank entries out of the _pollfds array.
	    for (pollfd *p = _pollfds.begin(); p < _pollfds.end(); p++)
		if (p->fd < 0) {
		    *p = _pollfds.back();
		    _pollfds.pop_back();
		    p--;
		}
	}
    }
#endif
	
#if !HAVE_POLL_H
    // Add 'mask' to the fd_sets
    if (fd < FD_SETSIZE) {
	if (mask & SELECT_READ)
	    FD_SET(fd, &_read_select_fd_set);
	if (mask & SELECT_WRITE)
	    FD_SET(fd, &_write_select_fd_set);
	if (fd > _max_select_fd)
	    _max_select_fd = fd;
    } else {
	static int warned = 0;
# if HAVE_SYS_EVENT_H && HAVE_KQUEUE
	if (_kqueue < 0)
# endif
	    if (!warned) {
		click_chatter("Master::add_select(%d): fd > FD_SETSIZE", fd);
		warned = 1;
	    }
    }
#endif

    _select_lock.release();
    return 0;
}

void
Master::remove_pollfd(int pi, int event)
{
    assert(event == POLLIN || event == POLLOUT);

    // remove event
    _pollfds[pi].events &= ~event;
    if (event == POLLIN)
	_read_poll_elements[pi] = 0;
    else
	_write_poll_elements[pi] = 0;

#if HAVE_SYS_EVENT_H && HAVE_KQUEUE
    // remove event from kqueue
    if (_kqueue >= 0) {
	struct kevent kev;
	EV_SET(&kev, _pollfds[pi].fd, (event == POLLIN ? EVFILT_READ : EVFILT_WRITE), EV_DELETE, 0, 0, EV_SET_UDATA_CAST ((intptr_t) pi));
	int r = kevent(_kqueue, &kev, 1, 0, 0, 0);
	if (r < 0)
	    click_chatter("Master::remove_pollfd(fd %d): kevent: %s", _pollfds[pi].fd, strerror(errno));
    }
#endif
#if !HAVE_POLL_H
    // remove event from select list
    if (_pollfds[pi].fd < FD_SETSIZE) {
	fd_set *fd_ptr = (event == POLLIN ? &_read_select_fd_set : &_write_select_fd_set);
	FD_CLR(_pollfds[pi].fd, fd_ptr);
    }
#endif

    // exit unless there are no events left
    if (_pollfds[pi].events)
	return;
    
    // remove whole pollfd
#if HAVE_SYS_EVENT_H && HAVE_KQUEUE
    // except we don't need to under kqueue
    if (_kqueue >= 0) {
	_pollfds[pi].fd = -1;
	return;
    }
#endif
    
    _pollfds[pi] = _pollfds.back();
    _pollfds.pop_back();
    // 31.Oct.2003 - Peter Swain: keep fds and elements in sync
    _write_poll_elements[pi]  = _write_poll_elements.back();
    _write_poll_elements.pop_back();
    _read_poll_elements[pi]  = _read_poll_elements.back();
    _read_poll_elements.pop_back();
#if !HAVE_POLL_H
    if (!_pollfds.size())
	_max_select_fd = -1;
#endif
}

int
Master::remove_select(int fd, Element *element, int mask)
{
    if (fd < 0)
	return -1;
    assert(element && (mask & ~(SELECT_READ | SELECT_WRITE)) == 0);
    _select_lock.acquire();

#if !HAVE_POLL_H
    // Exit early if no selector defined
    if (fd < FD_SETSIZE
	&& (!(mask & SELECT_READ) || !FD_ISSET(fd, &_read_select_fd_set))
	&& (!(mask & SELECT_WRITE) || !FD_ISSET(fd, &_write_select_fd_set))) {
	_select_lock.release();
	return -1;
    }
#endif

    // Search for selector
    for (pollfd *p = _pollfds.begin(); p != _pollfds.end(); p++)
	if (p->fd == fd) {
	    int pi = p - _pollfds.begin();
	    // check what to remove before removing anything, since
	    // remove_pollfd() can rearrange the _pollfds array
	    bool remove_read = (mask & SELECT_READ) && (p->events & POLLIN) && _read_poll_elements[pi] == element;
	    bool remove_write = (mask & SELECT_WRITE) && (p->events & POLLOUT) && _write_poll_elements[pi] == element;
	    if (remove_read)
		remove_pollfd(pi, POLLIN);
	    if (remove_write)
		remove_pollfd(pi, POLLOUT);
	    _select_lock.release();
	    return (remove_read || remove_write ? 0 : -1);
	}

    _select_lock.release();
    return -1;
}


#if HAVE_SYS_EVENT_H && HAVE_KQUEUE
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
#  if SIZEOF_STRUCT_TIMESPEC == 8
    Timestamp t;
    struct timespec *wait_ptr = (struct timespec*) &t;
    if (!more_tasks) {
	t = next_timer_expiry();
	if (t.sec() == 0)
	    wait_ptr = 0;
	else if ((t -= Timestamp::now(), t.sec() >= 0))
	    // fix up subseconds <-> nanoseconds
	    t.set_subsec(Timestamp::subsec_to_nsec(t.subsec()));
	else
	    t.set(0, 0);
    }
#  else /* SIZEOF_STRUCT_TIMESPEC != 8 */
    struct timespec wait, *wait_ptr = &wait;
    wait.tv_sec = wait.tv_nsec = 0;
    if (!more_tasks) {
	Timestamp t = next_timer_expiry();
	if (t.sec() == 0)
	    wait_ptr = 0;
	else if ((t -= Timestamp::now(), t.sec() >= 0))
	    wait = t.timespec();
    }
#  endif /* SIZEOF_STRUCT_TIMESPEC == 8 */
# endif

    // Bump selected_callno
    _selected_callno++;
    if (_selected_callno == 0) { // be anal about wraparound
	memset(_selected_callnos.begin(), 0, _selected_callnos.size() * sizeof(int));
	_selected_callno++;
    }
    
    struct kevent kev[64];
    int n = kevent(_kqueue, 0, 0, &kev[0], 64, wait_ptr);
    int was_errno = errno;
    run_signals();

    if (n < 0 && was_errno != EINTR)
	perror("kevent");
    else if (n > 0)
	for (struct kevent *p = &kev[0]; p < &kev[n]; p++) {
	    Element *e = 0;
	    int pi = (intptr_t) p->udata;
	    // check _pollfds[pi].fd in case 'selected()' called
	    // remove_select()
	    if (_pollfds[pi].fd != (int) p->ident)
		/* do nothing */;
	    else if (p->filter == EVFILT_READ && (_pollfds[pi].events & POLLIN))
		e = _read_poll_elements[pi];
	    else if (p->filter == EVFILT_WRITE && (_pollfds[pi].events & POLLOUT))
		e = _write_poll_elements[pi];
	    if (e && (_selected_callnos[pi] != _selected_callno
		      || _read_poll_elements[pi] != _write_poll_elements[pi])) {
		e->selected(p->ident);
		_selected_callnos[pi] = _selected_callno;
	    }
	}
}
#endif /* HAVE_SYS_EVENT_H && HAVE_KQUEUE */

#if HAVE_POLL_H
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
	Timestamp t = next_timer_expiry();
	if (t.sec() == 0)
	    timeout = -1;
	else if ((t -= Timestamp::now(), t.sec() >= 0)) {
	    if (t.sec() >= INT_MAX / 1000)
		timeout = INT_MAX - 1000;
	    else
		timeout = t.msec1();
	}
    }
# endif /* CLICK_NS */

    int n = poll(_pollfds.begin(), _pollfds.size(), timeout);
    int was_errno = errno;
    run_signals();

    if (n < 0 && was_errno != EINTR)
	perror("poll");
    else if (n > 0)
	for (struct pollfd *p = _pollfds.begin(); p < _pollfds.end(); p++)
	    if (p->revents) {
		int pi = p - _pollfds.begin();

		// Beware: calling 'selected()' might call remove_select(),
		// causing disaster! Load everything we need out of the
		// vectors before calling out.

		int fd = p->fd;
		Element *read_elt = (p->revents & ~POLLOUT ? _read_poll_elements[pi] : 0);
		Element *write_elt = (p->revents & ~POLLIN ? _write_poll_elements[pi] : 0);

		if (read_elt)
		    read_elt->selected(fd);
		if (write_elt && write_elt != read_elt)
		    write_elt->selected(fd);

		// 31.Oct.2003 - Peter Swain: _pollfds may have grown or
		// shrunk!
		p = _pollfds.begin() + pi;
		if (p < _pollfds.end() && fd != p->fd)
		    p--;
	    }
}

#else /* !HAVE_POLL_H */
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
#  if SIZEOF_STRUCT_TIMEVAL == 8
    Timestamp t;
    struct timeval *wait_ptr = (struct timeval*) &t;
    if (!more_tasks) {
	t = next_timer_expiry();
	if (t.sec() == 0)
	    wait_ptr = 0;
	else if ((t -= Timestamp::now(), t.sec() >= 0))
	    // fix up subseconds <-> microseconds
	    t.set_subsec(Timestamp::subsec_to_usec(t.subsec()));
	else
	    t.set(0, 0);
    }
#  else /* SIZEOF_STRUCT_TIMEVAL != 8 */
    struct timeval wait, *wait_ptr = &wait;
    timerclear(&wait);
    if (!more_tasks) {
	Timestamp t = next_timer_expiry();
	if (t.sec() == 0)
	    wait_ptr = 0;
	else if ((t -= Timestamp::now(), t.sec() >= 0))
	    wait = t.timeval();
    }
#  endif /* SIZEOF_STRUCT_TIMEVAL == 8 */
# endif /* CLICK_NS */

    fd_set read_mask = _read_select_fd_set;
    fd_set write_mask = _write_select_fd_set;

    int n = select(_max_select_fd + 1, &read_mask, &write_mask, (fd_set*) 0, wait_ptr);
    int was_errno = errno;
    run_signals();
  
    if (n < 0 && was_errno != EINTR)
	perror("select");
    else if (n > 0)
	for (struct pollfd *p = _pollfds.begin(); p < _pollfds.end(); p++)
	    if (p->fd > FD_SETSIZE || FD_ISSET(p->fd, &read_mask) || FD_ISSET(p->fd, &write_mask)) {
		int pi = p - _pollfds.begin();

		// Beware: calling 'selected()' might call remove_select(),
		// causing disaster! Load everything we need out of the
		// vectors before calling out.

		int fd = p->fd;
		Element *read_elt = (fd > FD_SETSIZE || FD_ISSET(fd, &read_mask) ? _read_poll_elements[pi] : 0);
		Element *write_elt = (fd > FD_SETSIZE || FD_ISSET(fd, &write_mask) ? _write_poll_elements[pi] : 0);

		if (read_elt)
		    read_elt->selected(fd);
		if (write_elt && write_elt != read_elt)
		    write_elt->selected(fd);

		// 31.Oct.2003 - Peter Swain: _pollfds may have grown or
		// shrunk!
		p = _pollfds.begin() + pi;
		if (p < _pollfds.end() && fd != p->fd)
		    p--;
	    }
}
#endif /* HAVE_POLL_H */

void
Master::run_selects(bool more_tasks)
{
    // Wait in select() for input or timer, and call relevant elements'
    // selected() methods.

    if (!_select_lock.attempt())
	return;

    // Return early if paused.
    if (_master_paused > 0)
	goto unlock_select_exit;

    // Return early if there are no selectors and there are tasks to run.
    if (_pollfds.size() == 0 && more_tasks)
	goto unlock_select_exit;

    // Call the relevant selector implementation.
#if HAVE_SYS_EVENT_H && HAVE_KQUEUE
    if (_kqueue >= 0) {
	run_selects_kqueue(more_tasks);
	goto unlock_select_exit;
    }
#endif
#if HAVE_POLL_H
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
    if (signo >= 0 && signo < 32)
	Master::signals_pending |= (1 << signo);
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
    
#if HAVE_SIGACTION
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sighandler;
    sa.sa_flags = SA_RESETHAND;
    sigaction(signo, &sa, 0);
#else
    signal(signo, sighandler);
#endif

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
	signal(signo, SIG_DFL);
    _signal_lock.release();
    return status;
}

void
Master::process_signals()
{
    uint32_t had_signals = signals_pending.swap(0);
    uint32_t unhandled_signals = had_signals;
    _signal_lock.acquire();

    SignalInfo *happened = 0;
    SignalInfo **pprev = &_siginfo;
    for (SignalInfo *si = *pprev; si; si = *pprev)
	if ((had_signals & (1 << si->signo)) && si->router->running()) {
	    *pprev = si->next;
	    si->next = happened;
	    happened = si;
	} else
	    pprev = &si->next;

    while (happened) {
	SignalInfo *next = happened->next;
	if (HandlerCall::call_write(happened->handler, happened->router->root_element()) >= 0)
	    unhandled_signals &= ~(1 << happened->signo);
	delete happened;
	happened = next;
    }

    sigset_t sigset;
    sigemptyset(&sigset);
    for (int signo = 0; signo < 32; signo++)
	if (had_signals & (1 << signo))
	    sigaddset(&sigset, signo);
    sigprocmask(SIG_UNBLOCK, &sigset, 0);

    for (int signo = 0; unhandled_signals; signo++)
	if (unhandled_signals & (1 << signo)) {
	    unhandled_signals &= ~(1 << signo);
	    kill(getpid(), signo);
	}

    _signal_lock.release();
}

#endif


// NS

#if CLICK_NS

void
Master::initialize_ns(simclick_sim siminst, simclick_click clickinst)
{
    assert(!_siminst && !_clickinst);
    _siminst = siminst;
    _clickinst = clickinst;
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
    sa << "pending:\t" << (Task::pending_to_task(_pending_head[0]) || Task::pending_to_task(_pending_head[1])) << '\n';
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


#if CLICK_USERLEVEL
// Vector template instance
# include <click/vector.cc>
#endif
CLICK_ENDDECLS
