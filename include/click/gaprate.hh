// -*- c-basic-offset: 4; related-file-name: "../../lib/gaprate.cc" -*-
#ifndef CLICK_GAPRATE_HH
#define CLICK_GAPRATE_HH
#include <click/timestamp.hh>
CLICK_DECLS
class ErrorHandler;

/** @file <click/gaprate.hh>
 *  @brief  A Click helper class for implementing a uniform rate.
 */

/** @class GapRate include/click/gaprate.hh <click/gaprate.hh>
 *  @brief  A helper class for implementing a uniform rate.
 *
 *  The GapRate class helps its user implement a process with a uniform rate:
 *  a process in which one user event happens every T seconds.  GapRate is
 *  designed to efficiently model high rates.  (Contrast this with Timer,
 *  which can serve a similar function at low rates.)
 *
 *  GapRate is not a great choice for limiting the rates of external
 *  processes.  See TokenBucketX, a token bucket rate limiter.
 *
 *  GapRate models an underlying "true" rated process with the correct rate.
 *  It also keeps a counter, maintained by its user via the update() method,
 *  that measures the progress of the user's rated process.  The
 *  needs_update() method compares this counter with its expected value, which
 *  is determined by the true rated process.  If the user's rated process is
 *  running behind, then needs_update() returns true; the user should trigger
 *  an event and call the update() method.  If the user's rated process is
 *  just right or is running faster than the true process, then needs_update()
 *  returns false; the user should <em>not</em> trigger an event.
 *
 *  Creating a non-bursty rate can be expensive and difficult.  GapRate
 *  attempts to create a non-bursty rate using timestamps and some interesting
 *  microsecond-based arithmetic.  External factors can cause scheduling
 *  hiccups where GapRate is not called as much as expected.  GapRate
 *  compensates for hiccups: the user's rated process may fall up to a full
 *  second behind the true rated process, then catch up in a burst.  More than
 *  one second's worth of lag is ignored.
 *
 *  The maximum rate GapRate can implement is MAX_RATE events per second.
 *
 *  @sa  TokenBucketX, Timer
 */
class GapRate { public:

    /** @brief  Construct a GapRate object with initial rate 0. */
    inline GapRate();

    /** @brief  Construct a GapRate object with initial rate @a r.
     *  @param  r  initial rate (events per second) */
    inline GapRate(unsigned r);

    /** @brief  Return the current rate. */
    inline unsigned rate() const;

    /** @brief  Set the current rate to @a r.
     *  @param  r  desired rate (events per second)
     *
     *  Rates larger than MAX_RATE are reduced to MAX_RATE.  Also performs the
     *  equivalent of a reset() to flush old state. */
    inline void set_rate(unsigned r);

    /** @brief  Set the current rate to @a r.
     *  @param  r     desired rate (events per second)
     *  @param  errh  error handler
     *
     *  Acts like set_rate(@a r), except that an warning is reported to @a errh
     *  if @a r is larger than MAX_RATE. */
    void set_rate(unsigned r, ErrorHandler *errh);


    /** @brief  Returns whether the user's rate is behind the true rate.
     *  @param  ts  current timestamp
     *
     *  Returns true iff the user's rate is currently behind the true rate,
     *  meaning the user should cause an event and call update(). */
    inline bool need_update(const Timestamp &ts);

    /** @brief  Returns a time when the user's rate will be behind the true rate.
     *  @pre  need_update() has been called at least once.
     *  @return If the rate is 0, or need_update() has not been called,
     *  returns Timestamp().  If the user's rate is already behind the true
     *  rate, returns a time no greater than the argument passed to the last
     *  need_update().  Otherwise, returns a time in the future when
     *  need_update() will return true.
     */
    inline Timestamp expiry() const;

    /** @brief  Increment the user event counter.
     *
     *  Call this function when causing a user event. */
    inline void update();

    /** @brief  Increment the user event counter by @a delta.
     *  @param  delta  number of user events
     *
     *  @note This may be faster than calling update() @a delta times.
     *  Furthermore, @a delta can be negative. */
    inline void update_with(int delta);

    /** @brief  Resets the true rate counter.
     *
     *  This function flushes any old information about the true rate counter
     *  and its relationship to the user's rate counter. */
    inline void reset();


    enum { UGAP_SHIFT = 12 };
    enum { MAX_RATE = 1000000U << UGAP_SHIFT };

  private:

    unsigned _ugap;		// (1000000 << UGAP_SHIFT) / _rate
    int _sec_count;		// number of updates this second so far
    Timestamp::seconds_type _tv_sec;	// current second
    unsigned _rate;		// desired rate
#if DEBUG_GAPRATE
    Timestamp _last;
#endif

    inline void initialize_rate(unsigned rate);

};

/** @brief  Reset the underlying rated process. */
inline void
GapRate::reset()
{
    _tv_sec = -1;
#if DEBUG_GAPRATE
    _last.set_sec(0);
#endif
}

inline void
GapRate::initialize_rate(unsigned r)
{
    _rate = r;
    _ugap = (r == 0 ? MAX_RATE + 1 : MAX_RATE / r);
#if DEBUG_GAPRATE
    click_chatter("ugap: %u", _ugap);
#endif
}

inline void
GapRate::set_rate(unsigned r)
{
    if (r > MAX_RATE)
	r = MAX_RATE;
    if (_rate != r) {
	initialize_rate(r);
	if (_tv_sec >= 0 && r != 0) {
	    Timestamp now = Timestamp::now();
	    _sec_count = (now.usec() << UGAP_SHIFT) / _ugap;
	}
    }
}

inline
GapRate::GapRate()
{
    initialize_rate(0);
    reset();
}

inline
GapRate::GapRate(unsigned r)
    : _rate(0)
{
    initialize_rate(r);
    reset();
}

inline unsigned
GapRate::rate() const
{
    return _rate;
}

inline bool
GapRate::need_update(const Timestamp &now)
{
    // this is an approximation of:
    // unsigned need = (unsigned) ((now.usec() / 1000000.0) * _rate)
    unsigned need = (now.usec() << UGAP_SHIFT) / _ugap;

    if (_tv_sec < 0) {
	// 27.Feb.2005: often OK to send a packet after reset unless rate is
	// 0 -- requested by Bart Braem
	// check include/click/gaprate.hh (1.2)
	_tv_sec = now.sec();
	_sec_count = need + ((now.usec() << UGAP_SHIFT) - (need * _ugap) > _ugap / 2);
    } else if (now.sec() > _tv_sec) {
	_tv_sec = now.sec();
	if (_sec_count > 0)
	    _sec_count -= _rate;
    }

#if DEBUG_GAPRATE
    click_chatter("%p{timestamp} -> %u @ %u [%d]", &now, need, _sec_count, (int)need >= _sec_count);
#endif
    return ((int)need >= _sec_count);
}

inline void
GapRate::update()
{
    _sec_count++;
}

inline void
GapRate::update_with(int delta)
{
    _sec_count += delta;
}

inline Timestamp
GapRate::expiry() const
{
    if (_tv_sec < 0 || _rate == 0)
        return Timestamp();
    else if (_sec_count < 0)
	return Timestamp(_tv_sec, 0);
    else {
	Timestamp::seconds_type sec = _tv_sec;
	int count = _sec_count;
	if ((unsigned) count >= _rate) {
	    sec += count / _rate;
	    count = count % _rate;
	}

	// uint32_t usec = (int) (count * 1000000.0 / _rate)
	uint32_t usec = (count * _ugap) >> UGAP_SHIFT;
	return Timestamp::make_usec(sec, usec);
    }
}

CLICK_ENDDECLS
#endif
