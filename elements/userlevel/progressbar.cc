// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * progressbar.{cc,hh} -- element displays a progress bar on stderr
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include "progressbar.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif
#include <termios.h>

ProgressBar::ProgressBar()
    : _timer(this)
{
    MOD_INC_USE_COUNT;
}

ProgressBar::~ProgressBar()
{
    MOD_DEC_USE_COUNT;
}

int
ProgressBar::configure(const Vector<String> &, ErrorHandler *)
{
    return 0;
}

int
ProgressBar::initialize(ErrorHandler *errh)
{
    Vector<String> conf;
    configuration(conf, false);
    _interval = 1000;

    if (cp_va_parse(conf, this, errh,
		    cpReadHandler, "position handler", &_pos_element, &_pos_hi,
		    cpReadHandler, "size handler", &_size_element, &_size_hi,
		    cpKeywords,
		    "UPDATE", cpSecondsAsMilli, "update interval (s)", &_interval,
		    "BANNER", cpString, "banner string", &_banner,
		    0) < 0)
	return -1;

    _have_size = false;
    _status = ST_FIRST;
    _timer.initialize(this);
    _timer.schedule_now();
    return 0;
}

void
ProgressBar::uninitialize()
{
    if (_status != ST_FIRST) {
	_status = ST_DONE;
	run_scheduled();
    }
    _timer.unschedule();
}

// from openssh scp
static bool
foregroundproc()
{
    static pid_t pgrp = -1;
    int ctty_pgrp;

    if (pgrp == -1)
	pgrp = getpgrp();

#ifdef HAVE_TCGETPGRP
    return ((ctty_pgrp = tcgetpgrp(STDOUT_FILENO)) != -1 &&
	    ctty_pgrp == pgrp);
#else
    return ((ioctl(STDOUT_FILENO, TIOCGPGRP, &ctty_pgrp) != -1 &&
	     ctty_pgrp == pgrp));
#endif
}

static int
getttywidth()
{
    // set TTY width (from openssh scp)
    struct winsize winsize;
    if (ioctl(fileno(stderr), TIOCGWINSZ, &winsize) != -1 && winsize.ws_col)
	return (winsize.ws_col ? winsize.ws_col : 80);
    else
	return 80;
}

static const char * const bar = 
"************************************************************"
"************************************************************"
"************************************************************"
"************************************************************";
static const char * const bad_bar =
"------------------------------------------------------------"
"------------------------------------------------------------"
"------------------------------------------------------------"
"------------------------------------------------------------";
static const int max_bar_length = 240;
static const char prefixes[] = " KMGTP";

#if 0
ssize_t
atomicio(f, fd, _s, n)
	ssize_t (*f) ();
	int fd;
	void *_s;
	size_t n;
{
	char *s = _s;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = (f) (fd, s + pos, n - pos);
		switch (res) {
		case -1:
#ifdef EWOULDBLOCK
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
#else
			if (errno == EINTR || errno == EAGAIN)
#endif
				continue;
		case 0:
			return (res);
		default:
			pos += res;
		}
	}
	return (pos);
}
#endif

#define STALLTIME	5

void
ProgressBar::run_scheduled()
{
    // get size on first time through
    if (_status == ST_FIRST) {
	String s = cp_uncomment(router()->handler(_size_hi).call_read(_size_element));
#ifdef HAVE_INT64_TYPES
	_have_size = cp_unsigned64(s, &_size);
#else
	_have_size = cp_unsigned(s, &_size);
#endif
	_last_pos = 0;
	click_gettimeofday(&_start_time);
	_last_time = _start_time;
	timerclear(&_stall_time);
    }

    // exit if not in foreground
    if (!foregroundproc()) {
	_timer.reschedule_after_ms(_interval);
	if (_status == ST_FIRST)
	    _status = ST_MIDDLE;
	return;
    }

    // start sa
    StringAccum sa;
    sa << "\r";
    if (_banner)
	sa << _banner << ' ';
    
    // get position
    String s = cp_uncomment(router()->handler(_pos_hi).call_read(_pos_element));
    thermometer_t pos;
#ifdef HAVE_INT64_TYPES
    bool have_pos = cp_unsigned64(s, &pos);
#else
    bool have_pos = cp_unsigned(s, &pos);
#endif

    // get current time
    struct timeval now;
    click_gettimeofday(&now);

    // measure how far along we are
    int thermpos;
    if (!have_pos)
	thermpos = -1;
    else if (!_have_size) {
	thermpos = (pos / 100000) % 200;
	if (thermpos > 100) thermpos = 200 - thermpos;
    } else if (_size > 0) {
	thermpos = (int)(100.0 * pos / _size);
	if (thermpos < 0) thermpos = 0;
	else if (thermpos > 100) thermpos = 100;
    } else
	thermpos = 100;

    // print percentage
    if (have_pos && _have_size)
	sa.snprintf(6, "%3d%% ", thermpos);
    else if (_have_size)
	sa << "  -% ";

    // print the bar
    int barlength = getttywidth() - (sa.length() + 25);
    barlength = (barlength <= max_bar_length ? barlength : max_bar_length);
    if (barlength > 0) {
	int barchar = (barlength * thermpos / 100);
	if (thermpos < 0 || (!_have_size && _status == ST_DONE))
	    sa.snprintf(barlength + 10, "|%.*s|", barlength, bad_bar);
	else if (!_have_size && barlength > 3) {
	    barchar = ((barlength - 2) * thermpos / 100);
	    sa.snprintf(barlength + 10, "|%*s***%*s|", barchar, "", barlength - barchar - 3, "");
	} else if (!_have_size)
	    sa.snprintf(barlength + 10, "|%*s*%*s|", barchar, "", barlength - barchar - 1, "");
	else
	    sa.snprintf(barlength + 10, "|%.*s%*s|", barchar, bar, barlength - barchar, "");
    }

    // print the size
    if (have_pos) {
	int which_pfx = 0;
	thermometer_t abbrevpos = pos;
	while (abbrevpos >= 100000 && which_pfx < (int)(sizeof(prefixes))) {
	    which_pfx++;
	    abbrevpos >>= 10;
	}
	sa.snprintf(30, " %5lu%c%c ", (unsigned long)abbrevpos, prefixes[which_pfx], (prefixes[which_pfx] == ' ' ? ' ' : 'B'));
    } else
	sa << " -----    ";

    // check wait time
    struct timeval wait;
    timersub(&now, &_last_time, &wait);
    if (pos > _last_pos) {
	_last_time = now;
	_last_pos = pos;
	if (wait.tv_sec >= STALLTIME)
	    timeradd(&_stall_time, &wait, &_stall_time);
	wait.tv_sec = 0;
    }

    // check elapsed time
    struct timeval tv;
    timersub(&now, &_start_time, &tv);
    timersub(&tv, &_stall_time, &tv);
    double elapsed = tv.tv_sec + (tv.tv_usec / 1000000.0);

    // collect time
    if (_status != ST_DONE
	&& (!_have_size || elapsed <= 0.0 || pos > _size))
	sa << "   --:-- ETA";
    else if (wait.tv_sec >= STALLTIME)
	sa << " - stalled -";
    else {
	int time_remaining;
	if (_status == ST_DONE)
	    time_remaining = (int)elapsed;
	else
	    time_remaining = (int)(_size / (pos / elapsed) - elapsed);
	
	int hr = time_remaining / 3600;
	if (hr)
	    sa.snprintf(12, "%2d:", hr);
	else
	    sa << "   ";
	int sec = time_remaining % 3600;
	sa.snprintf(12, "%02d:%02d%s", sec / 60, sec % 60,
		    (_status == ST_DONE ? "    " : " ETA"));
    }

    // add \n if appropriate
    if (_status == ST_DONE)
	sa << '\n';

    // write data
    int fd = fileno(stderr);
    int buflen = sa.length();
    int bufpos = 0;
    const char *data = sa.data();
    while (bufpos < buflen) {
	ssize_t got = write(fd, data + bufpos, buflen - bufpos);
	if (got > 0)
	    bufpos += got;
	else if (errno != EINTR && errno != EAGAIN)
	    break;
    }

    if (_status != ST_DONE)
	_timer.reschedule_after_ms(_interval);
    if (_status == ST_FIRST)
	_status = ST_MIDDLE;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ProgressBar)
