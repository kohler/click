// -*- c-basic-offset: 4; related-file-name: "../include/click/master.hh" -*-
/*
 * master.{cc,hh} -- Click event master
 * Eddie Kohler
 *
 * Copyright (c) 2003 The Regents of the University of California
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
#include <click/standard/drivermanager.hh>
#ifdef CLICK_USERLEVEL
# include <unistd.h>
#endif

CLICK_DECLS

Master::Master(int nthreads)
    : _master_paused(0), _routers(0), _task_list(0, 0), _timer_list(0, 0)
{
    _refcount = 0;
    _runcount = 0;

    for (int tid = -1; tid < nthreads; tid++)
	_threads.push_back(new RouterThread(this, tid));
    
    _task_list.make_list();
    _timer_list.make_list();
    
#if CLICK_USERLEVEL
# if !HAVE_POLL_H
    FD_ZERO(&_read_select_fd_set);
    FD_ZERO(&_write_select_fd_set);
    _max_select_fd = -1;
# endif
    assert(!_pollfds.size() && !_read_poll_elements.size() && !_write_poll_elements.size());
    // Add a null 'struct pollfd', then take it off. This ensures that
    // _pollfds.begin() is nonnull, preventing crashes on Mac OS X
    struct pollfd dummy;
    _pollfds.push_back(dummy);
    _pollfds.clear();
#endif

#if CLICK_NS
    _siminst = 0;
    _clickinst = 0;
#endif
}

Master::~Master()
{
    for (int i = 0; i < _threads.size(); i++)
	delete _threads[i];
    _timer_list.unmake_list();
}

void
Master::use()
{
    _master_lock.acquire();
    _refcount++;
    _master_lock.release();
}

void
Master::unuse()
{
    _master_lock.acquire();
    _refcount--;
    bool del = (_refcount <= 0);
    _master_lock.release();
    if (del)
	delete this;
}


// ROUTERS

void
Master::register_router(Router *router)
{
    _master_lock.acquire();
    _master_paused++;

    // add router to the list
    assert(router && router->_running == Router::RUNNING_INACTIVE && !router->_next_router);
    router->_running = Router::RUNNING_PAUSED;
    router->_next_router = _routers;
    _routers = router;
    _master_lock.release();
}

void
Master::run_router(Router *router)
{
    assert(router->_running == Router::RUNNING_PAUSED);
    _master_lock.acquire();
    router->_running = Router::RUNNING_ACTIVE;
    _master_paused--;
    _master_lock.release();
}

void
Master::remove_router(Router *router)
{
    _master_lock.acquire();
    int was_running = router->_running;
    router->_running = Router::RUNNING_DEAD;
    if (was_running == Router::RUNNING_ACTIVE)
	_master_paused++;
    else if (was_running == Router::RUNNING_PAUSED)
	/* nada */;
    else {
	assert(was_running == Router::RUNNING_INACTIVE || was_running == Router::RUNNING_DEAD);
	_master_lock.release();
	return;
    }
    _master_lock.release();

    // Remove router, fix runcount
    {
	_runcount_lock.acquire();
	_runcount = 0;
	Router **pprev = &_routers;
	bool found = false;
	for (Router *r = *pprev; r; r = r->_next_router)
	    if (r != router) {
		*pprev = r;
		pprev = &r->_next_router;
		if (r->_runcount > _runcount)
		    _runcount = r->_runcount;
	    } else
		found = true;
	*pprev = 0;
	_runcount_lock.release();
	if (!found) {
	    if (was_running == Router::RUNNING_ACTIVE) {
		_master_lock.acquire();
		_master_paused--;
		_master_lock.release();
	    }
	    return;
	}
    }
    
    // Remove tasks
    for (RouterThread **tp = _threads.begin(); tp < _threads.end(); tp++) {
	(*tp)->lock_tasks();
	Task *prev = *tp;
	Task *t;
	for (t = prev->_next; t != *tp; t = t->_next)
	    if (t->_router == router)
		t->_prev = 0;
	    else {
		prev->_next = t;
		t->_prev = prev;
		prev = t;
	    }
	prev->_next = t;
	t->_prev = prev;
	(*tp)->unlock_tasks();
    }
    
    {
	_task_lock.acquire();

	// Remove pending tasks
	Task *prev = &_task_list;
	for (Task *t = _task_list._pending_next; t != &_task_list; ) {
	    Task *next = t->_pending_next;
	    if (t->_router == router)
		t->_pending_next = 0;
	    else {
		prev->_pending_next = t;
		prev = t;
	    }
	    t = next;
	}
	prev->_pending_next = &_task_list;

	// Remove "all" tasks
	prev = &_task_list;
	Task *t;
	for (t = prev->_all_next; t != &_task_list; t = t->_all_next)
	    if (t->_router != router) {
		prev->_all_next = t;
		t->_all_prev = prev;
		prev = t;
	    }
	prev->_all_next = t;
	t->_all_prev = prev;
    
	_task_lock.release();
    }
    
    // Remove timers
    {
	_timer_lock.acquire();
	Timer *prev = &_timer_list;
	for (Timer *t = _timer_list._next; t != &_timer_list; t = t->_next)
	    if (t->_router == router) {
		t->_router = 0;
		t->_prev = 0;
	    } else {
		prev->_next = t;
		t->_prev = prev;
		prev = t;
	    }
	prev->_next = &_timer_list;
	_timer_list._prev = prev;
	_timer_lock.release();
    }

#if CLICK_USERLEVEL
    // Remove selects
    _select_lock.acquire();
    for (int pi = 0; pi < _pollfds.size(); pi++) {
	if (_read_poll_elements[pi] && _read_poll_elements[pi]->router() == router) {
	    _read_poll_elements[pi] = 0;
	    _pollfds[pi].events &= ~POLLIN;
	}
	if (_write_poll_elements[pi] && _write_poll_elements[pi]->router() == router) {
	    _write_poll_elements[pi] = 0;
	    _pollfds[pi].events &= ~POLLOUT;
	}
	if (_pollfds[pi].events == 0) {
	    remove_pollfd(pi);
	    pi--;
	}
    }
# if !HAVE_POLL_H
    FD_ZERO(&_read_select_fd_set);
    FD_ZERO(&_write_select_fd_set);
    _max_select_fd = -1;
    for (struct pollfd *p = _pollfds.begin(); p < _pollfds.end(); p++) {
	if (p->events & POLLIN)
	    FD_SET(p->fd, &_read_select_fd_set);
	if (p->events & POLLOUT)
	    FD_SET(p->fd, &_write_select_fd_set);
	if (p->fd > _max_select_fd)
	    _max_select_fd = p->fd;
    }
# endif
    _select_lock.release();
#endif

    _master_lock.acquire();
    _master_paused--;
    _master_lock.release();

    // something has happened, so wake up threads
    for (RouterThread **tp = _threads.begin(); tp < _threads.end(); tp++)
	(*tp)->unsleep();
}

bool
Master::check_driver()
{
    _master_lock.acquire();
    _runcount_lock.acquire();

    if (_runcount <= 0) {
	_runcount = 0;
	for (Router *r = _routers; r; r = r->_next_router) {
	    if (r->_runcount <= 0 && r->_running == Router::RUNNING_ACTIVE) {
		DriverManager *dm = (DriverManager *)(r->attachment("DriverManager"));
		if (dm)
		    while (1) {
			int was_runcount = _runcount;
			dm->handle_stopped_driver();
			if (r->_runcount <= was_runcount || r->_runcount > 0)
			    break;
		    }
	    }
	    if (r->_runcount > _runcount)
		_runcount = r->_runcount;
	}
    }
    
    bool more = (_runcount > 0);
    _runcount_lock.release();
    _master_lock.release();
    return more;
}


// Procedure for unscheduling a router:
// 


// PENDING TASKS

void
Master::process_pending(RouterThread *thread)
{
    if (_master_lock.attempt()) {
	if (_master_paused == 0) {
	    // get a copy of the list
	    _task_lock.acquire();
	    Task *t = _task_list._pending_next;
	    _task_list._pending_next = &_task_list;
	    thread->_pending = 0;
	    _task_lock.release();

	    // reverse list so pending tasks are processed in the order we
	    // added them
	    Task *prev = &_task_list;
	    while (t != &_task_list) {
		Task *next = t->_pending_next;
		t->_pending_next = prev;
		prev = t;
		t = next;
	    }

	    // process list
	    for (t = prev; t != &_task_list; ) {
		Task *next = t->_pending_next;
		t->_pending_next = 0;
		t->process_pending(thread);
		t = next;
	    }
	}
	_master_lock.release();
    }
}


// TIMERS

// How long until next timer expires.

int
Master::timer_delay(struct timeval *tv)
{
    int retval;
    _timer_lock.acquire();
    if (_timer_list._next == &_timer_list) {
	tv->tv_sec = 1000;
	tv->tv_usec = 0;
	retval = 0;
    } else {
	struct timeval now;
	click_gettimeofday(&now);
	if (timercmp(&_timer_list._next->_expiry, &now, >)) {
	    timersub(&_timer_list._next->_expiry, &now, tv);
	} else {
	    tv->tv_sec = 0;
	    tv->tv_usec = 0;
	}
	retval = 1;
    }
    _timer_lock.release();
    return retval;
}

void
Master::run_timers()
{
    if (_master_lock.attempt()) {
	if (_master_paused == 0 && _timer_lock.attempt()) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    while (_timer_list._next != &_timer_list
		   && !timercmp(&_timer_list._next->_expiry, &now, >)
		   && _runcount > 0) {
		Timer *t = _timer_list._next;
		_timer_list._next = t->_next;
		_timer_list._next->_prev = &_timer_list;
		t->_prev = 0;
		t->_hook(t, t->_thunk);
	    }
	    _timer_lock.release();
	}
	_master_lock.release();
    }
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
    assert(element && (mask & ~(SELECT_READ | SELECT_WRITE)) == 0);
    _select_lock.acquire();

    int si;
    for (si = 0; si < _pollfds.size(); si++)
	if (_pollfds[si].fd == fd) {
	    // There is exactly one match per fd.
	    if (((mask & SELECT_READ) && (_pollfds[si].events & POLLIN) && _read_poll_elements[si] != element)
		|| ((mask & SELECT_WRITE) && (_pollfds[si].events & POLLOUT) && _write_poll_elements[si] != element)) {
		_select_lock.release();
		return -1;
	    }
	    break;
	}

    // Add a new selector
    if (si == _pollfds.size()) {
	_pollfds.push_back(pollfd());
	_pollfds.back().fd = fd;
	_read_poll_elements.push_back(0);
	_write_poll_elements.push_back(0);
    }

    // Add selectors
    if (mask & SELECT_READ) {
	_pollfds[si].events |= POLLIN;
	_read_poll_elements[si] = element;
    }
    if (mask & SELECT_WRITE) {
	_pollfds[si].events |= POLLOUT;
	_write_poll_elements[si] = element;
    }

#if !HAVE_POLL_H
    // Add 'mask' to the fd_sets
    if (mask & SELECT_READ)
	FD_SET(fd, &_read_select_fd_set);
    if (mask & SELECT_WRITE)
	FD_SET(fd, &_write_select_fd_set);
    if ((mask & (SELECT_READ | SELECT_WRITE)) && fd > _max_select_fd)
	_max_select_fd = fd;
#endif

    _select_lock.release();
    return 0;
}

void
Master::remove_pollfd(int pi)
{
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
    if ((!(mask & SELECT_READ) || !FD_ISSET(fd, &_read_select_fd_set))
	&& (!(mask & SELECT_WRITE) || !FD_ISSET(fd, &_write_select_fd_set))) {
	_select_lock.release();
	return 0;
    }
#endif

    // Otherwise, search for selector
    for (pollfd *p = _pollfds.begin(); p != _pollfds.end(); p++)
	if (p->fd == fd) {
	    int pi = p - _pollfds.begin();
	    int ok = 0;
	    if ((mask & SELECT_READ) && (p->events & POLLIN) && _read_poll_elements[pi] == element) {
		p->events &= ~POLLIN;
#if !HAVE_POLL_H
		FD_CLR(fd, &_read_select_fd_set);
#endif
		ok++;
	    }
	    if ((mask & SELECT_WRITE) && (p->events & POLLOUT) && _write_poll_elements[pi] == element) {
		p->events &= ~POLLOUT;
#if !HAVE_POLL_H
		FD_CLR(fd, &_write_select_fd_set);
#endif
		ok++;
	    }
	    if (!p->events)
		remove_pollfd(pi);
	    _select_lock.release();
	    return (ok ? 0 : -1);
	}

    _select_lock.release();
    return -1;
}

void
Master::run_selects(bool more_tasks)
{
    // Wait in select() for input or timer, and call relevant elements'
    // selected() methods.

    if (!_master_lock.attempt())
	return;
    if (_master_paused > 0 || !_select_lock.attempt()) {
	_master_lock.release();
	return;
    }

    // Return early if there are no selectors and there are tasks to run.
    if (_pollfds.size() == 0 && more_tasks) {
	_select_lock.release();
	_master_lock.release();
	return;
    }

    // Decide how long to wait.
#if CLICK_NS
    // never block if we're running in the simulator
# if HAVE_POLL_H
    int timeout = -1;
# else
    struct timeval wait, *wait_ptr = &wait;
    timerclear(&wait);
# endif
#else /* !CLICK_NS */
    // never wait if anything is scheduled; otherwise, if no timers, block
    // indefinitely.
# if HAVE_POLL_H
    int timeout;
    if (more_tasks)
	timeout = 0;
    else {
	struct timeval wait;
	bool timers = timer_delay(&wait);
	timeout = (timers ? wait.tv_sec * 1000 + wait.tv_usec / 1000 : -1);
    }
# else /* !HAVE_POLL_H */
    struct timeval wait, *wait_ptr = &wait;
    if (more_tasks)
	timerclear(&wait);
    else if (!timer_delay(&wait))
	wait_ptr = 0;
# endif /* HAVE_POLL_H */
#endif /* CLICK_NS */

#if HAVE_POLL_H
    int n = poll(_pollfds.begin(), _pollfds.size(), timeout);

    if (n < 0 && errno != EINTR)
	perror("poll");
    else if (n > 0) {
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
    fd_set read_mask = _read_select_fd_set;
    fd_set write_mask = _write_select_fd_set;

    int n = select(_max_select_fd + 1, &read_mask, &write_mask, (fd_set *)0, wait_ptr);
  
    if (n < 0 && errno != EINTR)
	perror("select");
    else if (n > 0) {
	for (struct pollfd *p = _pollfds.begin(); p < _pollfds.end(); p++)
	    if (FD_ISSET(p->fd, &read_mask) || FD_ISSET(p->fd, &write_mask)) {
		int pi = p - _pollfds.begin();

		// Beware: calling 'selected()' might call remove_select(),
		// causing disaster! Load everything we need out of the
		// vectors before calling out.

		int fd = p->fd;
		Element *read_elt = (FD_ISSET(fd, &read_mask) ? _read_poll_elements[pi] : 0);
		Element *write_elt = (FD_ISSET(fd, &write_mask) ? _write_poll_elements[pi] : 0);

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

    _select_lock.release();
    _master_lock.release();
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
    sa << "runcount:\t" << _runcount << '\n';
    sa << "pending:\t" << (_task_list._pending_next != &_task_list) << '\n';
    for (int i = 0; i < _threads.size(); i++) {
	RouterThread *t = _threads[i];
	sa << "thread " << (i - 1) << ":";
	if (t->_sleeper)
	    sa << "\tsleep";
	else
	    sa << "\twake";
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
