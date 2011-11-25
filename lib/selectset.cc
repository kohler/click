// -*- c-basic-offset: 4; related-file-name: "../include/click/selectset.hh" -*-
/*
 * selectset.{cc,hh} -- Click set of file descriptor selectors
 * Eddie Kohler
 *
 * Copyright (c) 2003-2011 The Regents of the University of California
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
#include <click/selectset.hh>
#include <click/element.hh>
#include <click/task.hh>
#include <click/routerthread.hh>
#include <click/master.hh>
#include <fcntl.h>
#if HAVE_ALLOW_KQUEUE
# include <sys/event.h>
# if HAVE_EV_SET_UDATA_POINTER
#  define EV_SET_UDATA_CAST	(void *)
# else
#  define EV_SET_UDATA_CAST	/* nothing */
# endif
#endif
CLICK_DECLS

namespace {
enum { SELECT_READ = Element::SELECT_READ, SELECT_WRITE = Element::SELECT_WRITE };
#if !HAVE_ALLOW_POLL
enum { POLLIN = Element::SELECT_READ, POLLOUT = Element::SELECT_WRITE };
#endif
}

SelectSet::SelectSet()
{
    _wake_pipe_pending = false;
    _wake_pipe[0] = _wake_pipe[1] = -1;

#if HAVE_ALLOW_KQUEUE
# if defined(__APPLE__) && (HAVE_ALLOW_SELECT || HAVE_ALLOW_POLL)
    // Marc Lehmann's libev documentation, and some other online
    // documentation, indicate serious problems with kqueue on Mac OS X.  I've
    // observed delays even on 10.6.  So don't use it unless ordered.
    _kqueue = -1;
# else
    _kqueue = kqueue();
# endif
#endif

#if !HAVE_ALLOW_POLL
    FD_ZERO(&_read_select_fd_set);
    FD_ZERO(&_write_select_fd_set);
    _max_select_fd = -1;
#endif

    assert(!_pollfds.size() && !_selinfo.size());
    // Add a null 'struct pollfd', then take it off. This ensures that
    // _pollfds.begin() is nonnull, preventing crashes on Mac OS X
    struct pollfd dummy;
    dummy.events = dummy.fd = 0;
#if HAVE_ALLOW_POLL
    dummy.revents = 0;
#endif
    _pollfds.push_back(dummy);
    _pollfds.clear();

#if HAVE_MULTITHREAD
    _select_processor = click_invalid_processor();
#endif
}

SelectSet::~SelectSet()
{
#if HAVE_ALLOW_KQUEUE
    if (_kqueue >= 0)
	close(_kqueue);
#endif
    if (_wake_pipe[0] >= 0) {
	close(_wake_pipe[0]);
	close(_wake_pipe[1]);
    }
}

void
SelectSet::initialize()
{
    if (_wake_pipe[0] < 0 && pipe(_wake_pipe) >= 0) {
	fcntl(_wake_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(_wake_pipe[1], F_SETFL, O_NONBLOCK);
	fcntl(_wake_pipe[0], F_SETFD, FD_CLOEXEC);
	fcntl(_wake_pipe[1], F_SETFD, FD_CLOEXEC);
	register_select(_wake_pipe[0], true, false);
    }
    assert(_wake_pipe[0] >= 0);
}

void
SelectSet::kill_router(Router *router)
{
    lock();
    for (int pi = 0; pi < _pollfds.size(); pi++) {
	int fd = _pollfds[pi].fd;
	// take components out of the arrays early
	if (fd < _selinfo.size()) {
	    SelectorInfo &es = _selinfo.unchecked_at(fd);
	    if (es.read && es.read->router() == router)
		remove_pollfd(pi, POLLIN);
	    if (es.write && es.write->router() == router)
		remove_pollfd(pi, POLLOUT);
	}
	if (pi < _pollfds.size() && _pollfds[pi].fd != fd)
	    pi--;
    }
    unlock();
}

void
SelectSet::register_select(int fd, bool add_read, bool add_write)
{
    // add the pollfd
    if (fd >= _selinfo.size())
	_selinfo.resize(fd + 1);
    if (_selinfo[fd].pollfd < 0) {
	_selinfo[fd].pollfd = _pollfds.size();
	_pollfds.push_back(pollfd());
	_pollfds.back().fd = fd;
	_pollfds.back().events = 0;
    }
    int pi = _selinfo[fd].pollfd;

    // add the elements
    if (add_read)
	_pollfds[pi].events |= POLLIN;
    if (add_write)
	_pollfds[pi].events |= POLLOUT;

#if HAVE_ALLOW_KQUEUE
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

#if !HAVE_ALLOW_POLL
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
# if HAVE_ALLOW_KQUEUE
	if (_kqueue < 0)
# endif
	    if (!warned) {
		click_chatter("SelectSet::add_select(%d): fd >= FD_SETSIZE", fd);
		warned = 1;
	    }
    }
#endif

    // ensure the element selector exists
    if (fd >= _selinfo.size())
	_selinfo.resize(fd + 1);
}

int
SelectSet::add_select(int fd, Element *element, int mask)
{
    if (fd < 0)
	return -1;
    if (mask == 0)
	return 0;
    assert(element && (mask & ~(SELECT_READ | SELECT_WRITE)) == 0);
    lock();

    // check whether to add readability, writability, or both; it is an error
    // for more than one element to wait on the same fd for the same status
    bool add_read = false, add_write = false;
    if (mask & SELECT_READ) {
	if (fd >= _selinfo.size() || !_selinfo[fd].read)
	    add_read = true;
	else if (_selinfo[fd].read != element) {
	unlock_and_return_error:
	    unlock();
	    return -1;
	}
    }
    if (mask & SELECT_WRITE) {
	if (fd >= _selinfo.size() || !_selinfo[fd].write)
	    add_write = true;
	else if (_selinfo[fd].write != element)
	    goto unlock_and_return_error;
    }
    if (!add_read && !add_write) {
	unlock();
	return 0;
    }

    // add the pollfd
    register_select(fd, add_read, add_write);

    // add the elements
    if (add_read)
	_selinfo[fd].read = element;
    if (add_write)
	_selinfo[fd].write = element;

#if HAVE_MULTITHREAD
    // need to wake up selecting thread since there's more to select
    wake_immediate();
#endif

    unlock();
    return 0;
}

void
SelectSet::remove_pollfd(int pi, int event)
{
    assert(event == POLLIN || event == POLLOUT);

    // remove event
    int fd = _pollfds[pi].fd;
    _pollfds[pi].events &= ~event;
    if (event == POLLIN)
	_selinfo[fd].read = 0;
    else
	_selinfo[fd].write = 0;

#if HAVE_ALLOW_KQUEUE
    // remove event from kqueue
    if (_kqueue >= 0) {
	struct kevent kev;
	EV_SET(&kev, fd, (event == POLLIN ? EVFILT_READ : EVFILT_WRITE), EV_DELETE, 0, 0, EV_SET_UDATA_CAST ((intptr_t) 0));
	int r = kevent(_kqueue, &kev, 1, 0, 0, 0);
	if (r < 0)
	    click_chatter("SelectSet::remove_pollfd(fd %d): kevent: %s", _pollfds[pi].fd, strerror(errno));
    }
#endif
#if !HAVE_ALLOW_POLL
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
    _selinfo[fd].pollfd = -1;
    if (pi < _pollfds.size())
	_selinfo[_pollfds[pi].fd].pollfd = pi;
#if !HAVE_ALLOW_POLL
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
SelectSet::remove_select(int fd, Element *element, int mask)
{
    if (fd < 0)
	return -1;
    assert(element && (mask & ~(SELECT_READ | SELECT_WRITE)) == 0);
    lock();

    bool remove_read = false, remove_write = false;
    if ((mask & SELECT_READ) && fd < _selinfo.size()
	&& _selinfo[fd].read == element)
	remove_read = true;
    if ((mask & SELECT_WRITE) && fd < _selinfo.size()
	&& _selinfo[fd].write == element)
	remove_write = true;
    if (!remove_read && !remove_write) {
	unlock();
	return -1;
    }

    int pi = _selinfo[fd].pollfd;
    if (remove_read)
	remove_pollfd(pi, POLLIN);
    if (remove_write)
	remove_pollfd(pi, POLLOUT);
    unlock();
    return 0;
}

inline bool
SelectSet::post_select(RouterThread *thread, bool acquire)
{
#if HAVE_MULTITHREAD
    if (acquire) {
	_select_lock.acquire();
	_select_processor = click_current_processor();
    }
#else
    (void) acquire;
#endif

    if (_wake_pipe_pending) {
	_wake_pipe_pending = false;
	char crap[64];
	while (read(_wake_pipe[0], crap, 64) == 64)
	    /* do nothing */;
    }

    if (thread->master()->paused() || thread->stop_flag())
	return true;

    thread->run_signals();
    return false;
}

inline void
SelectSet::call_selected(int fd, int mask) const
{
    Element *read = 0, *write = 0;
    if ((unsigned) fd < (unsigned) _selinfo.size()) {
	const SelectorInfo &es = _selinfo[fd];
	if (mask & Element::SELECT_READ)
	    read = es.read;
	if (mask & Element::SELECT_WRITE)
	    write = es.write;
    }
    if (read)
	read->selected(fd, write == read ? mask : Element::SELECT_READ);
    if (write && write != read)
	write->selected(fd, Element::SELECT_WRITE);
}

#if HAVE_ALLOW_KQUEUE
static int
kevent_compare(const void *ap, const void *bp, void *)
{
    const struct kevent *a = static_cast<const struct kevent *>(ap);
    const struct kevent *b = static_cast<const struct kevent *>(bp);
    int afd = (int) a->ident, bfd = (int) b->ident;
    return afd - bfd;
}

void
SelectSet::run_selects_kqueue(RouterThread *thread)
{
# if HAVE_MULTITHREAD
    click_fence();
    _select_lock.release();
# endif

    // Decide how long to wait.
    struct timespec wait, *wait_ptr = &wait;
    Timestamp t;
    int delay_type = thread->timer_set().next_timer_delay(thread->active(), t);
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

    if (post_select(thread, true))
	return;

    thread->set_thread_state(RouterThread::S_RUNSELECT);
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
#endif /* HAVE_ALLOW_KQUEUE */

#if HAVE_ALLOW_POLL
void
SelectSet::run_selects_poll(RouterThread *thread)
{
# if HAVE_MULTITHREAD
    // Need a private copy of _pollfds, since other threads may run while we
    // block
    Vector<struct pollfd> my_pollfds(_pollfds);
    click_fence();
    _select_lock.release();
# else
    Vector<struct pollfd> &my_pollfds(_pollfds);
# endif

    // Decide how long to wait.
    int timeout;
    Timestamp t;
    int delay_type = thread->timer_set().next_timer_delay(thread->active(), t);
    if (delay_type == 0)
	timeout = 0;
    else if (delay_type > 0)
	timeout = (t.sec() >= INT_MAX / 1000 ? INT_MAX - 1000 : t.msecval());
    else
	timeout = -1;
    thread->set_thread_state_for_blocking(delay_type);

    int n = poll(my_pollfds.begin(), my_pollfds.size(), timeout);
    int was_errno = errno;

    if (post_select(thread, true))
	return;

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

#else /* !HAVE_ALLOW_POLL */
void
SelectSet::run_selects_select(RouterThread *thread)
{
    fd_set read_mask = _read_select_fd_set;
    fd_set write_mask = _write_select_fd_set;
    int n_select_fd = _max_select_fd + 1;

# if HAVE_MULTITHREAD
    click_fence();
    _select_lock.release();
# endif

    // Decide how long to wait.
    struct timeval wait, *wait_ptr = &wait;
    Timestamp t;
    int delay_type = thread->timer_set().next_timer_delay(thread->active(), t);
    if (delay_type == 0)
	timerclear(&wait);
    else if (delay_type > 0)
	wait = t.timeval();
    else
	wait_ptr = 0;
    thread->set_thread_state_for_blocking(delay_type);

    int n = select(n_select_fd, &read_mask, &write_mask, (fd_set*) 0, wait_ptr);
    int was_errno = errno;

    if (post_select(thread, true))
	return;

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
#endif /* HAVE_ALLOW_POLL */

void
SelectSet::run_selects(RouterThread *thread)
{
    // Wait in select() for input or timer, and call relevant elements'
    // selected() methods.

#if HAVE_MULTITHREAD
    if (!_select_lock.attempt())
	return;
#endif

    // Return early if paused.
    if (thread->master()->paused() || thread->stop_flag()) {
#if HAVE_MULTITHREAD
	_select_lock.release();
#endif
	return;
    }

    // Return early (just run signals) if there are no selectors and there are
    // tasks to run.  NB there will always be at least one _pollfd (the
    // _wake_pipe).
    if (_pollfds.size() < 2 && thread->active()) {
#if HAVE_MULTITHREAD
	_select_lock.release();
#endif
	post_select(thread, false);
	return;
    }

    // Call the relevant selector implementation.
    do {
#if HAVE_ALLOW_KQUEUE
	if (_kqueue >= 0) {
	    run_selects_kqueue(thread);
	    break;
	}
#endif
#if HAVE_ALLOW_POLL
	run_selects_poll(thread);
#else
	run_selects_select(thread);
#endif
    } while (0);

#if HAVE_MULTITHREAD
    _select_processor = click_invalid_processor();
    _select_lock.release();
#endif
}

CLICK_ENDDECLS
