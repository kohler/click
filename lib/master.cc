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
#ifdef CLICK_USERLEVEL
# include <unistd.h>
#endif

CLICK_DECLS

Master::Master()
{
#if CLICK_USERLEVEL
# if !HAVE_POLL_H
    FD_ZERO(&_read_select_fd_set);
    FD_ZERO(&_write_select_fd_set);
    _max_select_fd = -1;
# endif
    assert(!_pollfds.size() && !_read_poll_elements.size() && !_write_poll_elements.size());
#endif
}

Master::~Master()
{
    _timer_list.unschedule_all();
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

    int si;
    for (si = 0; si < _pollfds.size(); si++)
	if (_pollfds[si].fd == fd) {
	    // There is exactly one match per fd.
	    if (((mask & SELECT_READ) && (_pollfds[si].events & POLLIN) && _read_poll_elements[si] != element)
		|| ((mask & SELECT_WRITE) && (_pollfds[si].events & POLLOUT) && _write_poll_elements[si] != element))
		return -1;
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

    return 0;
}

int
Master::remove_select(int fd, Element *element, int mask)
{
    if (fd < 0)
	return -1;
    assert(element && (mask & ~(SELECT_READ | SELECT_WRITE)) == 0);

#if !HAVE_POLL_H
    // Exit early if no selector defined
    if ((!(mask & SELECT_READ) || !FD_ISSET(fd, &_read_select_fd_set))
	&& (!(mask & SELECT_WRITE) || !FD_ISSET(fd, &_write_select_fd_set)))
	return 0;
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
	    if (!p->events) {
		*p = _pollfds.back();
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
	    return (ok ? 0 : -1);
	}
  
    return -1;
}

void
Master::run_selects(bool more_tasks)
{
    // Wait in select() for input or timer, and call relevant elements'
    // selected() methods.

    // Return early if there are no selectors and there are tasks to run.
    if (_pollfds.size() == 0 && more_tasks)
	return;

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
	bool timers = _timer_list.get_next_delay(&wait);
	timeout = (timers ? wait.tv_sec * 1000 + wait.tv_usec / 1000 : -1);
    }
# else /* !HAVE_POLL_H */
    struct timeval wait, *wait_ptr = &wait;
    if (more_tasks)
	timerclear(&wait);
    else if (!_timer_list.get_next_delay(&wait))
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
		if (write_elt)
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
}

#endif


#if CLICK_USERLEVEL
// Vector template instance
# include <click/vector.cc>
#endif
CLICK_ENDDECLS
