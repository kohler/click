// -*- c-basic-offset: 4; related-file-name: "../include/click/timestamp.hh" -*-
/*
 * timestamp.{cc,hh} -- timestamps
 * Eddie Kohler
 *
 * Copyright (c) 2004-2008 Regents of the University of California
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
#include <click/timestamp.hh>
#include <click/straccum.hh>
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
# include <unistd.h>
# include <sys/ioctl.h>
#endif
CLICK_DECLS

/** @file timestamp.hh
 * @brief The Timestamp class represents a moment or interval in time.
 */

/** @class Timestamp
 @brief Represents a moment or interval in time.

 The Click Timestamp class represents both moments in time and intervals in
 time.  In most Click code, Timestamp replaces the Unix "struct timeval" and
 "struct timespec" structures; for example, Timer expiry times use the
 Timestamp class.  Timestamps may be added, subtracted, and compared using the
 usual operators.

 Timestamp measures time in seconds, and provides access to seconds and
 "subseconds", or fractions of a second.  Click can be configured with either
 microsecond or nanosecond precision.  Thus, one subsecond might equal either
 one microsecond or one nanosecond.  The subsec_per_sec enumeration constant
 equals the number of subseconds in a second; the timestamp's subsec() value
 should always lie between 0 and subsec_per_sec - 1.  (The
 <tt>--enable-nanotimestamp</tt> configuration option enables
 nanosecond-precision timestamps at user level; kernel modules use the
 kernel's native timestamp precision, which in later versions of Linux is
 nanosecond-precision.)

 A Timestamp with sec() < 0 is negative.  Note that subsec() is always
 nonnegative.  A Timestamp's value always equals (sec() + subsec() / (double)
 subsec_per_sec); thus, the Timestamp value of -0.1 is represented as sec() ==
 -1, usec() == +900000.
 */

#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
Timestamp::warp_class_type Timestamp::_warp_class = Timestamp::warp_none;
Timestamp Timestamp::_warp_flat_offset = Timestamp(0, 0);
double Timestamp::_warp_speed = 1.0;
double Timestamp::_warp_offset = 0.0;

void
Timestamp::warp(bool from_now)
{
    if (_warp_class == warp_simulation) {
	*this = _warp_flat_offset;
	if (from_now) {
# if TIMESTAMP_REP_FLAT64 || TIMESTAMP_MATH_FLAT64
	    ++_warp_flat_offset._t.x;
# else
	    ++_warp_flat_offset._t.subsec;
# endif
	    _warp_flat_offset.add_fix();
	}
    } else if (_warp_speed == 1.0)
	*this += _warp_flat_offset;
    else
	*this = Timestamp((doubleval() + _warp_offset) * _warp_speed);
}

void
Timestamp::warp_set_class(warp_class_type w)
{
    if (w == warp_none) {
	_warp_flat_offset.assign(0, 0);
	_warp_speed = 1.0;
	_warp_offset = 0.0;
    }
    _warp_class = w;
}

void
Timestamp::warp_adjust(const Timestamp &t_raw, const Timestamp &t_warped)
{
    if (_warp_class == warp_simulation)
	_warp_flat_offset = t_warped;
    else if (_warp_class == warp_nowait)
	_warp_flat_offset = t_warped - t_raw;
    else if (t_warped == t_raw && _warp_speed == 1.0)
	_warp_class = warp_none;
    else if (_warp_speed == 1.0) {
	_warp_class = warp_linear;
	_warp_flat_offset = t_warped - t_raw;
    } else
	_warp_offset = t_warped.doubleval() / _warp_speed - t_raw.doubleval();
}

void
Timestamp::warp_set_now(const Timestamp &t)
{
    Timestamp now_raw = Timestamp::uninitialized_t();
    now_raw.assign_now(true);
    warp_adjust(now_raw, t);
}

void
Timestamp::warp_set_speed(double f)
{
    assert(f > 0);
    Timestamp now_raw = Timestamp::uninitialized_t();
    now_raw.assign_now(true);
    Timestamp now_adj = now_raw.warped();
    _warp_speed = f;
    if (_warp_class < warp_nowait)
	warp_adjust(now_raw, now_adj);
}

void
Timestamp::warp_jump(const Timestamp &expiry)
{
    if (_warp_class == warp_simulation) {
	if (_warp_flat_offset < expiry)
	    _warp_flat_offset = expiry;
    } else if (_warp_class == warp_nowait) {
	Timestamp now_raw = Timestamp::uninitialized_t();
	now_raw.assign_now(true);
	if (now_raw.warped() < expiry)
	    warp_adjust(now_raw, expiry);
    }
}
#endif

#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
/** @brief Set this timestamp to a timeval obtained by calling ioctl.
    @param fd file descriptor
    @param ioctl_selector ioctl number

    Performs the same function as calling ioctl(@a fd, @a param, &tv) and
    setting *this = Timestamp(tv), where tv is a struct timeval, although it
    may be faster if Timestamp and struct timeval have the same
    representation. */
int
Timestamp::set_timeval_ioctl(int fd, int ioctl_selector)
{
    int r;
# if TIMESTAMP_PUNS_TIMEVAL
    r = ioctl(fd, ioctl_selector, this);
# elif SIZEOF_STRUCT_TIMEVAL == 8 && TIMESTAMP_REP_BIG_ENDIAN
    if ((r = ioctl(fd, ioctl_selector, this)) >= 0)
	_t.subsec = usec_to_subsec(_t.subsec);
# else
    struct timeval tv;
    if ((r = ioctl(fd, ioctl_selector, &tv)) >= 0)
	assign_usec(tv.tv_sec, tv.tv_usec);
# endif
    return r;
}
#endif


StringAccum &
operator<<(StringAccum &sa, const struct timeval &tv)
{
    if (char *x = sa.reserve(30)) {
	int len;
	if (tv.tv_sec >= 0)
	    len = sprintf(x, "%ld.%06ld", (long)tv.tv_sec, (long)tv.tv_usec);
	else if (tv.tv_usec == 0)
	    len = sprintf(x, "-%ld.%06ld", -(long)tv.tv_sec, (long)0);
	else
	    len = sprintf(x, "-%ld.%06ld", -((long)tv.tv_sec) - 1L, 1000000L - (long)tv.tv_usec);
	sa.adjust_length(len);
    }
    return sa;
}

/** @relates Timestamp
    @brief Append the unparsed representation of @a ts to @a sa.

    Same as @a sa @<@< @a ts.unparse(). */
StringAccum &
operator<<(StringAccum &sa, const Timestamp& ts)
{
    if (char *x = sa.reserve(33)) {
	Timestamp::seconds_type sec;
	uint32_t subsec;
	if (ts.sec() >= 0)
	    sec = ts.sec(), subsec = ts.subsec();
	else {
	    *x++ = '-';
	    sa.adjust_length(1);
	    if (ts.subsec() == 0)
		sec = -ts.sec(), subsec = 0;
	    else
		sec = -ts.sec() - 1, subsec = Timestamp::subsec_per_sec - ts.subsec();
	}

	int len;
#if TIMESTAMP_NANOSEC
	uint32_t usec = subsec / Timestamp::nsec_per_usec;
	if (usec * Timestamp::nsec_per_usec == subsec)
	    len = sprintf(x, "%ld.%06u", (long) sec, usec);
	else
	    len = sprintf(x, "%ld.%09u", (long) sec, subsec);
#else
	len = sprintf(x, "%ld.%06u", (long) sec, subsec);
#endif
	sa.adjust_length(len);
    }
    return sa;
}

/** @brief Unparse this timestamp into a String.

    Returns a string formatted like "10.000000", with at least six subsecond
    digits.  (Nanosecond-precision timestamps where the number of nanoseconds
    is not evenly divisible by 1000 are given nine subsecond digits.) */
String
Timestamp::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}

/** @brief Unparse this timestamp into a String as an interval.

    Returns a string formatted like "1us" or "1.000002s". */
String
Timestamp::unparse_interval() const
{
    StringAccum sa;
    if (sec() == 0) {
	uint32_t ss = subsec();
	if (ss % subsec_per_msec == 0)
	    sa << (ss / subsec_per_msec) << 'm' << 's';
#if TIMESTAMP_NANOSEC
	else if (ss % subsec_per_usec == 0)
	    sa << (ss / subsec_per_usec) << 'u' << 's';
	else
	    sa << ss << 'n' << 's';
#else
	else
	    sa << ss << 'u' << 's';
#endif
    } else
	sa << *this << 's';
    return sa.take_string();
}

CLICK_ENDDECLS
