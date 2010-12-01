// -*- c-basic-offset: 4; related-file-name: "../include/click/master.hh" -*-
/*
 * master.{cc,hh} -- Click event master
 * Eddie Kohler
 *
 * Copyright (c) 2003-7 The Regents of the University of California
 * Copyright (c) 2010 Intel Corporation
 * Copyright (c) 2008-2010 Meraki, Inc.
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
#include <click/heap.hh>
#if CLICK_USERLEVEL
# include <fcntl.h>
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
# if HAVE_MULTITHREAD
static RouterThread * volatile selecting_thread;
#  if HAVE___SYNC_SYNCHRONIZE
#   define click_master_mb()	__sync_synchronize()
#  else
#   define click_master_mb()	__asm__ volatile("" : : : "memory")
#  endif
# else
static int sig_pipe[2] = { -1, -1 };
# endif
extern "C" { static void sighandler(int signo); }
#endif

Master::Master(int nthreads)
    : _routers(0)
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
    selecting_thread = 0;
# endif

    // signal information
    signals_pending = 0;
    _siginfo = 0;
    sigemptyset(&_sig_dispatching);
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
    _master_paused++;
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
	for (Timer::heap_element *thp = _timer_heap.end();
	     thp > _timer_heap.begin(); ) {
	    --thp;
	    Timer *t = thp->t;
	    if (t->router() == router) {
		remove_heap<4>(_timer_heap.begin(), _timer_heap.end(), thp, Timer::heap_less(), Timer::heap_place());
		_timer_heap.pop_back();
		t->_owner = 0;
		t->_schedpos1 = 0;
	    }
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
Master::run_timers(RouterThread *thread)
{
    if (!attempt_lock_timers())
	return;
    if (_master_paused == 0 && _timer_heap.size() > 0 && !_stopper) {
	thread->set_thread_state(RouterThread::S_RUNTIMER);
#if CLICK_LINUXMODULE
	_timer_task = current;
#endif
	_timer_check = Timestamp::now();
	Timer::heap_element *th = _timer_heap.begin();

	if (th->expiry <= _timer_check) {
	    // potentially adjust timer stride
	    Timestamp adj_expiry = th->expiry + Timer::adjustment();
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
		Timer *t = th->t;
		pop_heap<4>(_timer_heap.begin(), _timer_heap.end(), Timer::heap_less(), Timer::heap_place());
		_timer_heap.pop_back();
		set_timer_expiry();
		t->_schedpos1 = 0;

		run_one_timer(t);
	    } while (_timer_heap.size() > 0 && !_stopper
		     && (th = _timer_heap.begin(), th->expiry <= _timer_check)
		     && --max_timers >= 0);

	    // If we ran out of timers to run, then perhaps there's an
	    // infinite timer loop or one timer is very far behind system
	    // time.  Eventually the system would catch up and run all timers,
	    // but in the meantime other timers could starve.  We detect this
	    // case and run ALL expired timers, reducing possible damage.
	    if (max_timers < 0 && !_stopper) {
		_timer_runchunk.reserve(32);
		do {
		    Timer *t = th->t;
		    pop_heap<4>(_timer_heap.begin(), _timer_heap.end(), Timer::heap_less(), Timer::heap_place());
		    _timer_heap.pop_back();
		    t->_schedpos1 = -_timer_runchunk.size() - 1;

		    _timer_runchunk.push_back(t);
		} while (_timer_heap.size() > 0
			 && (th = _timer_heap.begin(), th->expiry <= _timer_check));
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

void
Master::register_select(int fd, bool add_read, bool add_write)
{
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
    if (add_read)
	_pollfds[pi].events |= POLLIN;
    if (add_write)
	_pollfds[pi].events |= POLLOUT;

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

    // ensure the element selector exists
    if (fd >= _element_selectors.size())
	_element_selectors.resize(fd + 1);
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
    register_select(fd, add_read, add_write);

    // add the elements
    if (add_read)
	_element_selectors[fd].read = element;
    if (add_write)
	_element_selectors[fd].write = element;

#if HAVE_MULTITHREAD
    // need to wake up selecting thread since there's more to select
    if (RouterThread *r = selecting_thread)
	r->wake();
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


inline void
Master::call_selected(int fd, int mask) const
{
    if ((unsigned) fd < (unsigned) _element_selectors.size()) {
	const ElementSelector &es = _element_selectors[fd];
	Element *read = (mask & Element::SELECT_READ ? es.read : 0);
	Element *write = (mask & Element::SELECT_WRITE ? es.write : 0);
	if (read)
	    read->selected(fd, write == read ? mask : Element::SELECT_READ);
	if (write && write != read)
	    write->selected(fd, Element::SELECT_WRITE);
    }
}

inline int
Master::next_timer_delay(bool more_tasks, Timestamp &t) const
{
#if CLICK_NS
    // The simulator should never block.
    return 0;
#else
    if (more_tasks || signals_pending)
	return 0;
    t = next_timer_expiry_adjusted();
    if (t.sec() == 0)
	return -1;		// block forever
    else if (unlikely(Timestamp::warp_jumping())) {
	Timestamp::warp_jump(t);
	return 0;
    } else if ((t -= Timestamp::now(), t.sec() >= 0)) {
	t = t.warp_real_delay();
	return 1;
    } else
	return 0;
#endif
}

#if HAVE_USE_KQUEUE
static int
kevent_compare(const void *ap, const void *bp, void *)
{
    const struct kevent *a = static_cast<const struct kevent *>(ap);
    const struct kevent *b = static_cast<const struct kevent *>(bp);
    int afd = (int) a->ident, bfd = (int) b->ident;
    return afd - bfd;
}

void
Master::run_selects_kqueue(RouterThread *thread, bool more_tasks)
{
# if HAVE_MULTITHREAD
    selecting_thread = thread;
    click_master_mb();
    _select_lock.release();

    struct kevent wp_kev;
    EV_SET(&wp_kev, thread->_wake_pipe[0], EVFILT_READ, EV_ADD, 0, 0, EV_SET_UDATA_CAST ((intptr_t) 0));
    (void) kevent(_kqueue, &wp_kev, 1, 0, 0, 0);
# endif

    // Decide how long to wait.
    struct timespec wait, *wait_ptr = &wait;
    Timestamp t;
    int delay_type = next_timer_delay(more_tasks, t);
    if (delay_type == 0)
	wait.tv_sec = wait.tv_nsec = 0;
    else if (delay_type > 0)
	wait = t.timespec();
    else
	wait_ptr = 0;
    thread->set_thread_state_for_blocking(delay_type);

    struct kevent kev[256];
    int n = kevent(_kqueue, 0, 0, &kev[0], 256, wait_ptr);
    int was_errno = errno;
    run_signals(thread);

# if HAVE_MULTITHREAD
    thread->set_thread_state(RouterThread::S_LOCKSELECT);
    _select_lock.acquire();
    click_master_mb();
    selecting_thread = 0;
    thread->_select_blocked = false;

    thread->set_thread_state(RouterThread::S_RUNSELECT);
    wp_kev.flags = EV_DELETE;
    (void) kevent(_kqueue, &wp_kev, 1, 0, 0, 0);
# else
    thread->set_thread_state(RouterThread::S_RUNSELECT);
# endif

    if (n < 0 && was_errno != EINTR)
	perror("kevent");
    else if (n > 0) {
	click_qsort(&kev[0], n, sizeof(struct kevent), kevent_compare, 0);
	for (struct kevent *p = &kev[0]; p < &kev[n]; ) {
	    int fd = (int) p->ident, mask = 0;
	    for (; p < &kev[n] && (int) p->ident == fd; ++p)
		if (p->filter == EVFILT_READ)
		    mask |= Element::SELECT_READ;
		else if (p->filter == EVFILT_WRITE)
		    mask |= Element::SELECT_WRITE;
	    call_selected(fd, mask);
	}
    }
}
#endif /* HAVE_USE_KQUEUE */

#if HAVE_POLL_H && !HAVE_USE_SELECT
void
Master::run_selects_poll(RouterThread *thread, bool more_tasks)
{
# if HAVE_MULTITHREAD
    // Need a private copy of _pollfds, since other threads may run while we
    // block
    Vector<struct pollfd> my_pollfds(_pollfds);
    selecting_thread = thread;
    click_master_mb();
    _select_lock.release();

    pollfd wake_pollfd;
    wake_pollfd.fd = thread->_wake_pipe[0];
    wake_pollfd.events = POLLIN;
    my_pollfds.push_back(wake_pollfd);
# else
    Vector<struct pollfd> &my_pollfds(_pollfds);
# endif

    // Decide how long to wait.
    int timeout;
    Timestamp t;
    int delay_type = next_timer_delay(more_tasks, t);
    if (delay_type == 0)
	timeout = 0;
    else if (delay_type > 0)
	timeout = (t.sec() >= INT_MAX / 1000 ? INT_MAX - 1000 : t.msecval());
    else
	timeout = -1;
    thread->set_thread_state_for_blocking(delay_type);

    int n = poll(my_pollfds.begin(), my_pollfds.size(), timeout);
    int was_errno = errno;
    run_signals(thread);

# if HAVE_MULTITHREAD
    thread->set_thread_state(RouterThread::S_LOCKSELECT);
    _select_lock.acquire();
    click_master_mb();
    selecting_thread = 0;
    thread->_select_blocked = false;
# endif
    thread->set_thread_state(RouterThread::S_RUNSELECT);

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
		int mask = (p->revents & ~POLLOUT ? Element::SELECT_READ : 0)
		    + (p->revents & ~POLLIN ? Element::SELECT_WRITE : 0);
		call_selected(fd, mask);

		// 31.Oct.2003 - Peter Swain: _pollfds may have grown or
		// shrunk!
		p = my_pollfds.begin() + pi;
		if (p < my_pollfds.end() && fd != p->fd)
		    p--;
	    }
}

#else /* !HAVE_POLL_H || HAVE_USE_SELECT */
void
Master::run_selects_select(RouterThread *thread, bool more_tasks)
{
    fd_set read_mask = _read_select_fd_set;
    fd_set write_mask = _write_select_fd_set;
    int n_select_fd = _max_select_fd + 1;

# if HAVE_MULTITHREAD
    selecting_thread = thread;
    click_master_mb();
    _select_lock.release();

    FD_SET(&read_mask, thread->_wake_pipe[0]);
    if (thread->_wake_pipe[0] >= n_select_fd)
	n_select_fd = thread->_wake_pipe[0] + 1;
# endif

    // Decide how long to wait.
    struct timeval wait, *wait_ptr = &wait;
    Timestamp t;
    int delay_type = next_timer_delay(more_tasks, t);
    if (delay_type == 0)
	timerclear(&wait);
    else if (delay_type > 0)
	wait = t.timeval();
    else
	wait_ptr = 0;
    thread->set_thread_state_for_blocking(delay_type);

    int n = select(n_select_fd, &read_mask, &write_mask, (fd_set*) 0, wait_ptr);
    int was_errno = errno;
    run_signals(thread);

# if HAVE_MULTITHREAD
    thread->set_thread_state(RouterThread::S_LOCKSELECT);
    _select_lock.acquire();
    click_master_mb();
    selecting_thread = 0;
    thread->_select_blocked = false;
# endif
    thread->set_thread_state(RouterThread::S_RUNSELECT);

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
		int mask = (fd >= FD_SETSIZE || FD_ISSET(fd, &read_mask) ? Element::SELECT_READ : 0)
		    + (fd >= FD_SETSIZE || FD_ISSET(fd, &write_mask) ? Element::SELECT_WRITE : 0);
		call_selected(fd, mask);

		// 31.Oct.2003 - Peter Swain: _pollfds may have grown or
		// shrunk!
		p = _pollfds.begin() + pi;
		if (p < _pollfds.end() && fd != p->fd)
		    p--;
	    }
}
#endif /* HAVE_POLL_H && !HAVE_USE_SELECT */

void
Master::run_selects(RouterThread *thread)
{
    // Wait in select() for input or timer, and call relevant elements'
    // selected() methods.

    if (!_select_lock.attempt())
	return;

#if HAVE_MULTITHREAD
    // set _select_blocked to true first: then, if someone else is
    // concurrently waking us up, we will either detect that the thread is now
    // active(), or wake up on the write to the thread's _wake_pipe
    thread->_select_blocked = true;
    click_master_mb();
#endif

    bool more_tasks = thread->active();

#if HAVE_MULTITHREAD
    if (selecting_thread) {
	// Another thread is blocked in select().  No point in this thread's
	// also checking file descriptors, so block if there are no more tasks
	// to run.  It *is* useful to wait until the next timer expiry,
	// because we may have set some new short-term timers while the other
	// thread was blocking.
	_select_lock.release();
	if (!more_tasks) {
	    // watch _wake_pipe so the thread can be awoken
	    fd_set fdr;
	    FD_ZERO(&fdr);
	    FD_SET(thread->_wake_pipe[0], &fdr);

	    // how long to wait?
	    struct timeval wait, *wait_ptr = &wait;
	    Timestamp t;
	    int delay_type = next_timer_delay(false, t);
	    if (delay_type == 0)
		timerclear(&wait);
	    else if (delay_type > 0)
		wait = t.timeval();
	    else
		wait_ptr = 0;
	    thread->set_thread_state_for_blocking(delay_type);

	    // actually wait
	    int r = select(thread->_wake_pipe[0] + 1, &fdr,
			   (fd_set *) 0, (fd_set *) 0, wait_ptr);
	    (void) r;
	}
	run_signals(thread);
	thread->_select_blocked = false;
	return;
    }
#endif

    // Return early if paused.
    if (_master_paused > 0) {
#if HAVE_MULTITHREAD
	thread->_select_blocked = false;
#endif
	goto unlock_exit;
    }

    // Return early (just run signals) if there are no selectors and there are
    // tasks to run.
    if (_pollfds.size() == 0 && more_tasks) {
	run_signals(thread);
#if HAVE_MULTITHREAD
	thread->_select_blocked = false;
#endif
	goto unlock_exit;
    }

    // Call the relevant selector implementation.
#if HAVE_USE_KQUEUE
    if (_kqueue >= 0) {
	run_selects_kqueue(thread, more_tasks);
	goto unlock_exit;
    }
#endif
#if HAVE_POLL_H && !HAVE_USE_SELECT
    run_selects_poll(thread, more_tasks);
#else
    run_selects_select(thread, more_tasks);
#endif

 unlock_exit:
    _select_lock.release();
}

#endif


// SIGNALS

#if CLICK_USERLEVEL

extern "C" {
static void
sighandler(int signo)
{
    Master::signals_pending = signal_pending[signo] = 1;
# if HAVE_MULTITHREAD
    click_master_mb();
    if (selecting_thread)
	selecting_thread->wake();
# else
    if (sig_pipe[1] >= 0)
	ignore_result(write(sig_pipe[1], "", 1));
# endif
}
}

int
Master::add_signal_handler(int signo, Router *router, String handler)
{
    if (signo < 0 || signo >= NSIG || router->master() != this)
	return -1;

# if !HAVE_MULTITHREAD
    if (sig_pipe[0] < 0 && pipe(sig_pipe) >= 0) {
	fcntl(sig_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(sig_pipe[1], F_SETFL, O_NONBLOCK);
	fcntl(sig_pipe[0], F_SETFD, FD_CLOEXEC);
	fcntl(sig_pipe[1], F_SETFD, FD_CLOEXEC);
	register_select(sig_pipe[0], true, false);
    }
# endif

    _signal_lock.acquire();
    int status = 0, nhandlers = 0;
    SignalInfo **pprev = &_siginfo;
    for (SignalInfo *si = *pprev; si; si = *pprev)
	if (si->equals(signo, router, handler))
	    goto unlock_exit;
	else {
	    nhandlers += (si->signo == signo);
	    pprev = &si->next;
	}

    if ((*pprev = new SignalInfo(signo, router, handler))) {
	if (nhandlers == 0 && sigismember(&_sig_dispatching, signo) == 0)
	    click_signal(signo, sighandler, false);
    } else
	status = -1;

  unlock_exit:
    _signal_lock.release();
    return status;
}

int
Master::remove_signal_handler(int signo, Router *router, String handler)
{
    _signal_lock.acquire();
    int nhandlers = 0, status = -1;
    SignalInfo **pprev = &_siginfo;
    for (SignalInfo *si = *pprev; si; si = *pprev)
	if (si->equals(signo, router, handler)) {
	    *pprev = si->next;
	    delete si;
	    status = 0;
	} else {
	    nhandlers += (si->signo == signo);
	    pprev = &si->next;
	}

    if (status >= 0 && nhandlers == 0
	&& sigismember(&_sig_dispatching, signo) == 0)
	click_signal(signo, SIG_DFL, false);

    _signal_lock.release();
    return status;
}

static inline void
clear_pipe(int fd)
{
    if (fd >= 0) {
	char crap[64];
	while (read(fd, crap, 64) == 64)
	    /* do nothing */;
    }
}

void
Master::process_signals(RouterThread *thread)
{
    thread->set_thread_state(RouterThread::S_RUNSIGNAL);

    // kill crap data written to pipe
#if HAVE_MULTITHREAD
    if (thread->_wake_pipe_pending) {
	thread->_wake_pipe_pending = false;
	clear_pipe(thread->_wake_pipe[0]);
    }
#else
    if (signals_pending)
	clear_pipe(sig_pipe[0]);
#endif

    // exit early if still no signals
    if (!signals_pending)
	return;

    // otherwise, grab the signal lock
    signals_pending = 0;
    _signal_lock.acquire();

    // collect activated signal handler info
    SignalInfo *happened, **hpprev = &happened;
    for (SignalInfo **pprev = &_siginfo, *si = *pprev; si; si = *pprev)
	if ((signal_pending[si->signo]
	     || sigismember(&_sig_dispatching, si->signo) > 0)
	    && si->router->running()) {
	    sigaddset(&_sig_dispatching, si->signo);
	    signal_pending[si->signo] = 0;
	    *pprev = si->next;
	    *hpprev = si;
	    hpprev = &si->next;
	} else
	    pprev = &si->next;
    *hpprev = 0;

    // call relevant signal handlers
    sigset_t sigset_handled;
    sigemptyset(&sigset_handled);
    while (happened) {
	SignalInfo *next = happened->next;
	if (HandlerCall::call_write(happened->handler, happened->router->root_element()) >= 0)
	    sigaddset(&sigset_handled, happened->signo);
	delete happened;
	happened = next;
    }

    // collect currently active signal handlers (handler calls may have
    // changed this set)
    sigset_t sigset_active;
    sigemptyset(&sigset_active);
    for (SignalInfo *si = _siginfo; si; si = si->next)
	sigaddset(&sigset_active, si->signo);

    // reset & possibly redeliver unhandled signals and signals that we gave
    // up on that happened again since we started running this function
    for (int signo = 0; signo < NSIG; ++signo)
	if (sigismember(&_sig_dispatching, signo) > 0) {
	    if (sigismember(&sigset_active, signo) == 0) {
		click_signal(signo, SIG_DFL, false);
#if HAVE_MULTITHREAD
		click_master_mb();
#endif
		if (signal_pending[signo] != 0) {
		    signal_pending[signo] = 0;
		    goto suicide;
		}
	    }
	    if (sigismember(&sigset_handled, signo) == 0) {
	    suicide:
		kill(getpid(), signo);
	    }
	}

    sigemptyset(&_sig_dispatching);
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


#if CLICK_DEBUG_MASTER || CLICK_DEBUG_SCHEDULING
#include <click/straccum.hh>

String
Master::info() const
{
    StringAccum sa;
    sa << "paused:\t\t" << _master_paused << '\n';
    sa << "stopper:\t" << _stopper << '\n';
    for (int i = 0; i < _threads.size(); i++) {
	RouterThread *t = _threads[i];
	sa << "thread " << (i - 2) << ":";
# ifdef CLICK_LINUXMODULE
	if (t->_sleeper)
	    sa << "\tsleep";
	else
	    sa << "\twake";
# endif
	if (t->_pending_head)
	    sa << "\tpending";
# if CLICK_USERLEVEL && HAVE_MULTITHREAD
	if (t->_wake_pipe[0] >= 0) {
	    fd_set rfd;
	    struct timeval to;
	    FD_ZERO(&rfd);
	    FD_SET(t->_wake_pipe[0], &rfd);
	    timerclear(&to);
	    (void) select(t->_wake_pipe[0] + 1, &rfd, 0, 0, &to);
	    if (FD_ISSET(t->_wake_pipe[0], &rfd))
		sa << "\tpipewoken";
	}
# endif
# if CLICK_DEBUG_SCHEDULING
	sa << '\t' << RouterThread::thread_state_name(t->thread_state());
# endif
	sa << '\n';
# if CLICK_DEBUG_SCHEDULING > 1
	t->set_thread_state(t->thread_state()); // account for time
	bool any = false;
	for (int s = 0; s < RouterThread::NSTATES; ++s)
	    if (Timestamp time = t->thread_state_time(s)) {
		sa << (any ? ", " : "\t\t")
		   << RouterThread::thread_state_name(s)
		   << ' ' << time << '/' << t->thread_state_count(s);
		any = true;
	    }
	if (any)
	    sa << '\n';
# endif
    }
    return sa.take_string();
}

#endif
CLICK_ENDDECLS
