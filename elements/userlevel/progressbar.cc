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
#include <unistd.h>
#include <sys/ioctl.h>
CLICK_DECLS

ProgressBar::ProgressBar()
    : _status(ST_FIRST), _timer(this)
{
    MOD_INC_USE_COUNT;
}

ProgressBar::~ProgressBar()
{
    MOD_DEC_USE_COUNT;
}

int
ProgressBar::configure(Vector<String> &, ErrorHandler *)
{
    return 0;
}

int
ProgressBar::initialize(ErrorHandler *errh)
{
    Vector<String> conf;
    configuration(conf);
    _interval = 250;
    _delay_ms = 0;
    _active = true;
    String position_str, size_str;
    bool check_stdout = false;

    if (cp_va_parse(conf, this, errh,
		    cpArgument, "position handler", &position_str,
		    cpOptional,
		    cpArgument, "size handler", &size_str,
		    cpKeywords,
		    "UPDATE", cpSecondsAsMilli, "update interval (s)", &_interval,
		    "BANNER", cpString, "banner string", &_banner,
		    "ACTIVE", cpBool, "start active?", &_active,
		    "DELAY", cpSecondsAsMilli, "display delay (s)", &_delay_ms,
		    "CHECK_STDOUT", cpBool, "check if stdout is terminal?", &check_stdout,
		    0) < 0)
	return -1;

    Vector<String> words;
    cp_spacevec(size_str, words);
    _first_pos_h = words.size();
    cp_spacevec(position_str, words);
    _es.assign(words.size(), 0);
    _his.assign(words.size(), -1);

    for (int i = 0; i < words.size(); i++)
	if (!cp_handler(words[i], this, true, false, &_es[i], &_his[i], errh))
	    return -1;

    if (!isatty(STDERR_FILENO) || (check_stdout && isatty(STDOUT_FILENO)))
	_status = ST_DEAD;
    else
	_status = ST_FIRST;
    
    _have_size = false;
    _timer.initialize(this);
    if (_active && _status != ST_DEAD)
	_timer.schedule_now();
    return 0;
}

void
ProgressBar::cleanup(CleanupStage)
{
    if (_status == ST_MIDDLE) {
	_status = ST_DONE;
	run_timer();
    }
}


// Code based on the progress bar in the OpenSSH project's B<scp> program. Its
// authors are listed as Timo Rinne, Tatu Ylonen, Theo de Raadt, and Aaron
// Campbell. Under a BSD-like copyright.

static bool
foregroundproc(int fd)
{
    static pid_t pgrp = -1;
    int ctty_pgrp;

    if (pgrp == -1)
	pgrp = getpgrp();

#ifdef HAVE_TCGETPGRP
    return ((ctty_pgrp = tcgetpgrp(fd)) != -1 &&
	    ctty_pgrp == pgrp);
#else
    return ((ioctl(fd, TIOCGPGRP, &ctty_pgrp) != -1 &&
	     ctty_pgrp == pgrp));
#endif
}

static int
getttywidth()
{
    // set TTY width (from openssh scp)
    struct winsize winsize;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &winsize) != -1 && winsize.ws_col)
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

bool
ProgressBar::get_value(int first, int last, thermometer_t *value)
{
    *value = 0;
    bool all_known = true;
    for (int i = first; i < last; i++) {
	String s = cp_uncomment(router()->handler(_his[i]).call_read(_es[i]));
	thermometer_t this_value;
#ifdef HAVE_INT64_TYPES
	bool ok = cp_unsigned64(s, &this_value);
#else
	bool ok = cp_unsigned(s, &this_value);
#endif
	if (ok)
	    *value += this_value;
	else
	    all_known = false;
    }
    return (first == last ? false : all_known);
}

void
ProgressBar::run_timer()
{
    // check _active
    if (!_active || _status == ST_DEAD)
	return;
    
    // get size on first time through
    if (_status == ST_FIRST || _status == ST_FIRSTDONE) {
	if (!_have_size)
	    _have_size = get_value(0, _first_pos_h, &_size);
	_last_pos = 0;
	click_gettimeofday(&_start_time);
	_last_time = _start_time;
	timerclear(&_stall_time);
	struct timeval delay;
	delay.tv_sec = _delay_ms / 1000;
	delay.tv_usec = (_delay_ms % 1000) * 1000;
	timeradd(&_start_time, &delay, &_delay_time);
	if (_status == ST_FIRST)
	    _status = ST_MIDDLE;
    }

    // exit if not in foreground
    if (!foregroundproc(STDERR_FILENO)) {
	_timer.reschedule_after_ms(_interval);
	return;
    }

    // get current time
    struct timeval now;
    click_gettimeofday(&now);

    // exit if wait time not passed
    if (timercmp(&now, &_delay_time, <)) {
	_timer.reschedule_at(_delay_time);
	return;
    }
    
    // get position
    thermometer_t pos;
    bool have_pos = get_value(_first_pos_h, _es.size(), &pos);

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

    // start sa, print percentage
    StringAccum sa;
    sa << "\r";
    if (_banner)
	sa << _banner << ' ';
    if (have_pos && _have_size)
	sa.snprintf(6, "%3d%% ", thermpos);
    else if (_have_size)
	sa << "  -% ";

    // print the bar
    int barlength = getttywidth() - (sa.length() + 25);
    barlength = (barlength <= max_bar_length ? barlength : max_bar_length);
    if (barlength > 0) {
	if (thermpos < 0 || (!_have_size && _status >= ST_DONE))
	    sa.snprintf(barlength + 10, "|%.*s|", barlength, bad_bar);
	else if (!_have_size && barlength > 3) {
	    int barchar = ((barlength - 3) * thermpos / 100);
	    sa.snprintf(barlength + 10, "|%*s***%*s|", barchar, "", barlength - barchar - 3, "");
	} else if (!_have_size) {
	    int barchar = ((barlength - 1) * thermpos / 100);
	    sa.snprintf(barlength + 10, "|%*s*%*s|", barchar, "", barlength - barchar - 1, "");
	} else {
	    int barchar = (barlength * thermpos / 100);
	    sa.snprintf(barlength + 10, "|%.*s%*s|", barchar, bar, barlength - barchar, "");
	}
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
	sa << " -----   ";

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
    if (_status < ST_DONE
	&& (!_have_size || elapsed <= 0.0 || pos > _size))
	sa << "   --:-- ETA";
    else if (wait.tv_sec >= STALLTIME)
	sa << " - stalled -";
    else {
	int time_remaining;
	if (_status >= ST_DONE)
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
		    (_status >= ST_DONE ? "    " : " ETA"));
    }

    // add \n if appropriate
    if (_status >= ST_DONE)
	sa << '\n';

    // write data
    int fd = STDERR_FILENO;
    int buflen = sa.length();
    int bufpos = 0;
    const char *data = sa.data();
    while (bufpos < buflen) {
	ssize_t got = write(fd, data + bufpos, buflen - bufpos);
	if (got > 0)
	    bufpos += got;
	else if (errno != EINTR && errno != EAGAIN
#ifdef EWOULDBLOCK
		 && errno != EWOULDBLOCK
#endif
		 )
	    break;
    }

    if (_status < ST_DONE)
	_timer.reschedule_after_ms(_interval);
    else
	_active = false;
}

void
ProgressBar::complete(bool is_full)
{
    if (_status < ST_DONE && _active) {
	if (is_full) {
	    _have_size = true;
	    (void) get_value(_first_pos_h, _es.size(), &_size);
	}
	_status = (_status == ST_FIRST ? ST_FIRSTDONE : ST_DONE);
	_timer.unschedule();
	run_timer();
    }
}


enum { H_MARK_STOPPED, H_MARK_DONE, H_BANNER, H_ACTIVE,
       H_POSHANDLER, H_SIZEHANDLER, H_RESET };

String
ProgressBar::read_handler(Element *e, void *thunk)
{
    ProgressBar *pb = static_cast<ProgressBar *>(e);
    switch ((intptr_t)thunk) {
      case H_BANNER:
	return pb->_banner + "\n";
      case H_ACTIVE:
	return cp_unparse_bool(pb->_active) + "\n";
      case H_POSHANDLER:
      case H_SIZEHANDLER: {
	  bool is_pos = ((intptr_t)thunk == H_POSHANDLER);
	  StringAccum sa;
	  for (int i = (is_pos ? pb->_first_pos_h : 0); i < (is_pos ? pb->_es.size() : pb->_first_pos_h); i++) {
	      if (sa.length()) sa << ' ';
	      sa << pb->router()->handler(pb->_his[i]).unparse_name(pb->_es[i]);
	  }
	  sa << '\n';
	  return sa.take_string();
      }
      default:
	return "<error>";
    }
}

int
ProgressBar::write_handler(const String &in_str, Element *e, void *thunk, ErrorHandler *errh)
{
    ProgressBar *pb = static_cast<ProgressBar *>(e);
    String str = cp_uncomment(in_str);
    switch ((intptr_t)thunk) {
      case H_MARK_STOPPED:
	pb->complete(false);
	return 0;
      case H_MARK_DONE:
	pb->complete(true);
	return 0;
      case H_BANNER:
	pb->_banner = str;
	return 0;
      case H_POSHANDLER:
      case H_SIZEHANDLER: {
	  Vector<String> words;
	  cp_spacevec(str, words);
	  bool is_pos = ((intptr_t)thunk == H_POSHANDLER);
	  int total = (is_pos ? pb->_first_pos_h + words.size() : pb->_es.size() - pb->_first_pos_h + words.size());
	  int offset = (is_pos ? pb->_first_pos_h : 0);
	  
	  Vector<Element *> es;
	  Vector<int> his;
	  es.assign(total, 0);
	  his.assign(total, -1);

	  for (int i = 0; i < words.size(); i++)
	      if (!cp_handler(words[i], pb, true, false, &es[i + offset], &his[i + offset], errh))
		  return -1;

	  offset = (is_pos ? 0 : words.size() - pb->_first_pos_h);
	  for (int i = (is_pos ? 0 : pb->_first_pos_h); i < (is_pos ? pb->_first_pos_h : pb->_es.size()); i++)
	      es[i + offset] = pb->_es[i], his[i + offset] = pb->_his[i];
	  
	  es.swap(pb->_es);
	  his.swap(pb->_his);
	  if (!is_pos) {
	      pb->_have_size = false;
	      pb->_first_pos_h = words.size();
	  }
	  return 0;
      }
      case H_ACTIVE:
	if (cp_bool(str, &pb->_active)) {
	    if (pb->_active && !pb->_timer.scheduled())
		pb->_timer.schedule_now();
	    return 0;
	} else
	    return errh->error("`active' should be bool (active setting)");
      case H_RESET:
	pb->_have_size = false;
	pb->_status = ST_FIRST;
	pb->_active = true;
	pb->_timer.schedule_now();
	return 0;
      default:
	return errh->error("internal");
    }
}

void
ProgressBar::add_handlers()
{
    add_write_handler("mark_stopped", write_handler, (void *)H_MARK_STOPPED);
    add_write_handler("mark_done", write_handler, (void *)H_MARK_DONE);
    add_read_handler("active", read_handler, (void *)H_ACTIVE);
    add_write_handler("active", write_handler, (void *)H_ACTIVE);
    add_read_handler("banner", read_handler, (void *)H_BANNER);
    add_write_handler("banner", write_handler, (void *)H_BANNER);
    add_read_handler("poshandler", read_handler, (void *)H_POSHANDLER);
    add_write_handler("poshandler", write_handler, (void *)H_POSHANDLER);
    add_read_handler("sizehandler", read_handler, (void *)H_SIZEHANDLER);
    add_write_handler("sizehandler", write_handler, (void *)H_SIZEHANDLER);
    add_write_handler("reset", write_handler, (void *)H_RESET);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ProgressBar)
