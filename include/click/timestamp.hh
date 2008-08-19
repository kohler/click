// -*- c-basic-offset: 4; related-file-name: "../../lib/timestamp.cc" -*-
#ifndef CLICK_TIMESTAMP_HH
#define CLICK_TIMESTAMP_HH
#include <click/glue.hh>
CLICK_DECLS
class String;
#if !HAVE_NANOTIMESTAMP && SIZEOF_STRUCT_TIMEVAL == 8
# define TIMESTAMP_PUNS_TIMEVAL 1
#endif
#if HAVE_STRUCT_TIMESPEC && HAVE_NANOTIMESTAMP && SIZEOF_STRUCT_TIMESPEC == 8
# define TIMESTAMP_PUNS_TIMESPEC 1
#endif
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
# include <math.h>
#endif
#if (CLICK_USERLEVEL || CLICK_TOOL) && !CLICK_NS && HAVE_DECL_CLOCK_GETTIME && HAVE_CLOCK_GETTIME
# define CLICK_USE_CLOCK_GETTIME 1
#endif
#if TIMESTAMP_PUNS_INT64
# define TIMESTAMP_CONSTRUCTOR(s, ss, t) _x((t) (s) * NSUBSEC + (ss))
#else
# define TIMESTAMP_CONSTRUCTOR(s, ss, t) _sec(s), _subsec(ss)
#endif

#if HAVE_NANOTIMESTAMP
# define PRITIMESTAMP "%d.%09d"
#else
# define PRITIMESTAMP "%d.%06d"
#endif

class Timestamp { public:

    /** @brief  Type represents a number of seconds. */
    typedef int32_t seconds_type;

#if HAVE_NANOTIMESTAMP
    enum {
	NSUBSEC = 1000000000	/**< Number of subseconds in a second.  Can be
				     1000000 or 1000000000, depending on how
				     Click is compiled. */
    };
#else
    enum {
	NSUBSEC = 1000000
    };
#endif

    enum {
	max_seconds = (seconds_type) 2147483647U,
	min_seconds = (seconds_type) -2147483648U
    };

    struct uninitialized_t {
    };


    /** @brief Construct a zero-valued Timestamp. */
    inline Timestamp()
	: TIMESTAMP_CONSTRUCTOR(0, 0, int) {
    }

    /** @brief Construct a Timestamp of @a sec seconds plus @a subsec
     *     subseconds.
     * @param sec number of seconds
     * @param subsec number of subseconds (defaults to 0)
     *
     * The @a subsec parameter must be between 0 and NSUBSEC - 1, and the @a
     * sec parameter must be between @link Timestamp::min_seconds min_seconds
     * @endlink and @link Timestamp::max_seconds max_seconds @endlink.  Errors
     * are not necessarily checked. */
    inline Timestamp(int sec, uint32_t subsec = 0)
	: TIMESTAMP_CONSTRUCTOR(sec, subsec, int64_t) {
    }
    /** @overload */
    inline Timestamp(long sec, uint32_t subsec = 0)
	: TIMESTAMP_CONSTRUCTOR(sec, subsec, int64_t) {
    }
    /** @overload */
    inline Timestamp(unsigned sec, uint32_t subsec = 0)
	: TIMESTAMP_CONSTRUCTOR(sec, subsec, uint64_t) {
    }
    /** @overload */
    inline Timestamp(unsigned long sec, uint32_t subsec = 0)
	: TIMESTAMP_CONSTRUCTOR(sec, subsec, uint64_t) {
    }

    inline Timestamp(const struct timeval &tv);
#if HAVE_STRUCT_TIMESPEC
    inline Timestamp(const struct timespec &ts);
#endif
#if HAVE_FLOAT_TYPES
    inline Timestamp(double);
#endif
    /** @brief Construct an uninitialized timestamp. */
    inline Timestamp(const uninitialized_t &unused) {
	(void) unused;
    }

    typedef seconds_type (Timestamp::*unspecified_bool_type)() const;
    inline operator unspecified_bool_type() const;
    
    inline seconds_type sec() const;
    inline uint32_t subsec() const;
    
    inline uint32_t msec() const;
    inline uint32_t usec() const;
    inline uint32_t nsec() const;
    
    inline seconds_type msec1() const;
    inline seconds_type usec1() const;
    inline seconds_type nsec1() const;

#if TIMESTAMP_PUNS_TIMEVAL
    inline const struct timeval &timeval() const;
#else
    inline struct timeval timeval() const;
#endif
#if HAVE_STRUCT_TIMESPEC
# if TIMESTAMP_PUNS_TIMESPEC
    inline const struct timespec &timespec() const;
# else
    inline struct timespec timespec() const;
# endif
#endif
#if HAVE_FLOAT_TYPES
    inline double doubleval() const;
#endif

#if !CLICK_TOOL
    /** @brief Return a timestamp representing an interval of @a jiffies. */
    static inline Timestamp make_jiffies(click_jiffies_t jiffies);
    /** @brief Return the number of jiffies represented by this timestamp.
     *
     * Treats the current timestamp as an interval (rather than an absolute
     * timestamp). */
    inline click_jiffies_t jiffies() const;
#endif

#if TIMESTAMP_PUNS_INT64
    inline int64_t value() const {
	return _x;
    }

    static inline Timestamp make_value(int64_t x) {
	Timestamp t = uninitialized_t();
	t._x = x;
	return t;
    }
#endif

    static inline Timestamp make_sec(seconds_type sec);
    static inline Timestamp make_msec(unsigned long msec);
    static inline Timestamp make_msec(long msec);
    static inline Timestamp make_msec(unsigned msec);
    static inline Timestamp make_msec(int msec);
    static inline Timestamp make_usec(seconds_type sec, uint32_t usec);
    static inline Timestamp make_usec(unsigned long usec);
    static inline Timestamp make_usec(unsigned usec);
    static inline Timestamp make_usec(long usec);
    static inline Timestamp make_usec(int usec);
    static inline Timestamp make_nsec(seconds_type sec, uint32_t nsec);
    
    static inline Timestamp epsilon();
    static inline Timestamp now();

    inline void assign(seconds_type sec, uint32_t subsec = 0);
    inline void set(seconds_type sec, uint32_t subsec = 0);
    inline void set_sec(seconds_type sec);
    inline void set_subsec(uint32_t subsec);
    inline void set_usec(seconds_type sec, uint32_t usec);
    inline void set_nsec(seconds_type sec, uint32_t nsec);

    inline void set_now();
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
    int set_timeval_ioctl(int fd, int ioctl_selector);
#endif

    String unparse() const;

    inline static uint32_t msec_to_subsec(uint32_t msec);
    inline static uint32_t usec_to_subsec(uint32_t usec);
    inline static uint32_t nsec_to_subsec(uint32_t nsec);
    inline static uint32_t subsec_to_msec(uint32_t subsec);
    inline static uint32_t subsec_to_usec(uint32_t subsec);
    inline static uint32_t subsec_to_nsec(uint32_t subsec);
    
  private:

#if TIMESTAMP_PUNS_INT64
    int64_t _x;
#else
    seconds_type _sec;
    int32_t _subsec;

    void add_fix();
    void sub_fix();
#endif

    friend inline Timestamp &operator+=(Timestamp &a, const Timestamp &b);
    friend inline Timestamp &operator-=(Timestamp &a, const Timestamp &b);

};


/** @brief Convert milliseconds to subseconds.
 @param msec number of milliseconds

 Converts its parameter to a number of subseconds (that is, either
 microseconds or nanoseconds, depending on configuration options and driver
 choice).

 @sa usec_to_subsec(), nsec_to_subsec(), subsec_to_msec(), subsec_to_usec(),
 subsec_to_nsec() */
inline uint32_t
Timestamp::msec_to_subsec(uint32_t msec)
{
#if HAVE_NANOTIMESTAMP
    return msec * 1000000;
#else
    return msec * 1000;
#endif
}

/** @brief Convert microseconds to subseconds.
 @param usec number of microseconds
 @sa msec_to_subsec() */
inline uint32_t
Timestamp::usec_to_subsec(uint32_t usec)
{
#if HAVE_NANOTIMESTAMP
    return usec * 1000;
#else
    return usec;
#endif
}

/** @brief Convert nanoseconds to subseconds.
 @param nsec number of nanoseconds
 @sa msec_to_subsec() */
inline uint32_t
Timestamp::nsec_to_subsec(uint32_t nsec)
{
#if HAVE_NANOTIMESTAMP
    return nsec;
#else
    return (nsec + 500) / 1000;
#endif
}

/** @brief Convert subseconds to milliseconds.
 @param subsec number of subseconds
 @sa msec_to_subsec() */
inline uint32_t
Timestamp::subsec_to_msec(uint32_t subsec)
{
#if HAVE_NANOTIMESTAMP
    return (subsec + 500000) / 1000000;
#else
    return (subsec + 500) / 1000;
#endif
}

/** @brief Convert subseconds to microseconds.
 @param subsec number of subseconds
 @sa msec_to_subsec() */
inline uint32_t
Timestamp::subsec_to_usec(uint32_t subsec)
{
#if HAVE_NANOTIMESTAMP
    return (subsec + 500) / 1000;
#else
    return subsec;
#endif
}

/** @brief Convert subseconds to nanoseconds.
 @param subsec number of subseconds
 @sa msec_to_subsec() */
inline uint32_t
Timestamp::subsec_to_nsec(uint32_t subsec)
{
#if HAVE_NANOTIMESTAMP
    return subsec;
#else
    return subsec * 1000;
#endif
}

/** @brief Create a Timestamp measuring @a tv.
    @param tv timeval structure */
inline
Timestamp::Timestamp(const struct timeval& tv)
    : TIMESTAMP_CONSTRUCTOR(tv.tv_sec, usec_to_subsec(tv.tv_usec), int64_t)
{
}

#if HAVE_STRUCT_TIMESPEC
/** @brief Create a Timestamp measuring @a ts.
    @param ts timespec structure */
inline
Timestamp::Timestamp(const struct timespec& ts)
    : TIMESTAMP_CONSTRUCTOR(ts.tv_sec, nsec_to_subsec(ts.tv_nsec), int64_t)
{
}
#endif

/** @brief Return true iff this timestamp is not zero-valued. */
inline
Timestamp::operator unspecified_bool_type() const
{
#if TIMESTAMP_PUNS_INT64
    return _x ? &Timestamp::sec : 0;
#else
    return _sec || _subsec ? &Timestamp::sec : 0;
#endif
}

/** @brief Set this timestamp's components.
    @param sec number of seconds
    @param subsec number of subseconds */
inline void
Timestamp::assign(seconds_type sec, uint32_t subsec)
{
#if TIMESTAMP_PUNS_INT64
    _x = (int64_t) sec * NSUBSEC + subsec;
#else
    _sec = sec;
    _subsec = subsec;
#endif
}

/** @brief Set this timestamp's components.
    @param sec number of seconds
    @param subsec number of subseconds */
inline void
Timestamp::set(seconds_type sec, uint32_t subsec)
{
#if TIMESTAMP_PUNS_INT64
    _x = (int64_t) sec * NSUBSEC + subsec;
#else
    _sec = sec;
    _subsec = subsec;
#endif
}

/** @brief Set this timestamp to the current time.

 The current time is measured in seconds since January 1, 1970 GMT.
 Returns the most precise timestamp available.
 @sa now() */
inline void
Timestamp::set_now()
{
#if HAVE_NANOTIMESTAMP && (!CLICK_USERLEVEL || CLICK_USE_CLOCK_GETTIME)
    // nanosecond precision
# if TIMESTAMP_PUNS_TIMESPEC
    struct timespec *tsp = (struct timespec *) this;
# else
    struct timespec ts, *tsp = &ts;
# endif
# if CLICK_LINUXMODULE
    getnstimeofday(tsp);
# elif CLICK_BSDMODULE
    nanotime(tsp);		// This is the more precise function
# elif CLICK_USERLEVEL || CLICK_TOOL
    clock_gettime(CLOCK_REALTIME, tsp);
# else
#  error "unknown driver"
# endif
# if !TIMESTAMP_PUNS_TIMESPEC
    assign(ts.tv_sec, nsec_to_subsec(ts.tv_nsec));
# endif

#else
    // microsecond precision
# if TIMESTAMP_PUNS_TIMEVAL
    struct timeval *tvp = (struct timeval *) this;
# else
    struct timeval tv, *tvp = &tv;
# endif
# if CLICK_LINUXMODULE
    do_gettimeofday(tvp);
# elif CLICK_BSDMODULE
    microtime(tvp);
# elif CLICK_NS
    simclick_gettimeofday(tvp);
# elif CLICK_USERLEVEL || CLICK_TOOL
    gettimeofday(tvp, (struct timezone *) 0);
# else
#  error "unknown driver"
# endif
# if !TIMESTAMP_PUNS_TIMEVAL
    assign(tv.tv_sec, usec_to_subsec(tv.tv_usec));
# endif
#endif
}

/** @brief Return a timestamp representing the current time.

 The current time is measured in seconds since January 1, 1970 GMT.
 @sa set_now() */
inline Timestamp
Timestamp::now()
{
    Timestamp t = Timestamp::uninitialized_t();
    t.set_now();
    return t;
}

/** @brief Return the smallest nonzero timestamp.

 Same as Timestamp(0, 1). */
inline Timestamp
Timestamp::epsilon()
{
    return Timestamp(0, 1);
}

/** @brief Return a timestamp representing an interval of @a sec
    seconds.
    @param sec number of seconds */
inline Timestamp
Timestamp::make_sec(seconds_type sec)
{
    return Timestamp(sec, 0);
}

/** @brief Return a timestamp representing an interval of @a msec
    milliseconds.
    @param msec number of milliseconds (may be greater than 1000) */
inline Timestamp
Timestamp::make_msec(unsigned long msec)
{
#if TIMESTAMP_PUNS_INT64
    static_assert(NSUBSEC == 1000000000);
    return Timestamp::make_value((uint64_t) msec * 1000000);
#else
    return Timestamp(msec / 1000, msec_to_subsec(msec % 1000));
#endif
}

/** @brief Return a timestamp representing an interval of @a msec
    milliseconds.
    @param msec number of milliseconds (may be negative or greater than 1000) */
inline Timestamp
Timestamp::make_msec(long msec)
{
#if TIMESTAMP_PUNS_INT64
    static_assert(NSUBSEC == 1000000000);
    return Timestamp::make_value((int64_t) msec * 1000000);
#else
    if (msec < 0) {
	int32_t s = -(-msec / 1000);
	int32_t ss = msec_to_subsec(-msec % 1000);
	if (ss == 0)
	    return Timestamp(s);
	else
	    return Timestamp(s - 1, NSUBSEC - ss);
    } else
	return Timestamp(msec / 1000, msec_to_subsec(msec % 1000));
#endif
}

/** @overload */
inline Timestamp
Timestamp::make_msec(unsigned msec)
{
    return make_msec(static_cast<unsigned long>(msec));
}

/** @overload */
inline Timestamp
Timestamp::make_msec(int msec)
{
    return make_msec(static_cast<long>(msec));
}

/** @brief Return a timestamp representing @a sec seconds plus @a usec
    microseconds.
    @param sec number of seconds
    @param usec number of microseconds (less than 1000000) */
inline Timestamp
Timestamp::make_usec(seconds_type sec, uint32_t usec)
{
    return Timestamp(sec, usec_to_subsec(usec));
}

/** @brief Return a timestamp representing an interval of @a usec
    microseconds.
    @param usec number of microseconds (may be greater than 1000000) */
inline Timestamp
Timestamp::make_usec(unsigned long usec)
{
#if TIMESTAMP_PUNS_INT64
    static_assert(NSUBSEC == 1000000000);
    return Timestamp::make_value((uint64_t) usec * 1000);
#else
    return Timestamp(usec / 1000000, usec_to_subsec(usec % 1000000));
#endif
}

/** @brief Return a timestamp representing an interval of @a usec
    microseconds.
    @param usec number of microseconds (may be negative or greater than 1000000) */
inline Timestamp
Timestamp::make_usec(long usec)
{
#if TIMESTAMP_PUNS_INT64
    static_assert(NSUBSEC == 1000000000);
    return Timestamp::make_value((int64_t) usec * 1000);
#else
    if (usec < 0) {
	int32_t s = -(-usec / 1000000);
	int32_t ss = -usec % 1000000;
	if (ss == 0)
	    return Timestamp(s);
	else
	    return Timestamp(s - 1, NSUBSEC - ss);
    } else
	return Timestamp(usec / 1000000, usec_to_subsec(usec % 1000000));
#endif
}

/** @overload */
inline Timestamp
Timestamp::make_usec(unsigned usec)
{
    return make_usec(static_cast<unsigned long>(usec));
}

/** @overload */
inline Timestamp
Timestamp::make_usec(int usec)
{
    return make_usec(static_cast<long>(usec));
}

/** @brief Return a timestamp representing @a sec seconds plus @a nsec
    nanoseconds.
    @param sec number of seconds
    @param nsec number of nanoseconds (less than 1000000000) */
inline Timestamp
Timestamp::make_nsec(seconds_type sec, uint32_t nsec)
{
    return Timestamp(sec, nsec_to_subsec(nsec));
}

/** @brief Set this timestamp's seconds component.
    @param sec number of seconds
    
    The subseconds component is left unchanged. */
inline void
Timestamp::set_sec(seconds_type sec)
{
#if TIMESTAMP_PUNS_INT64
    uint32_t ss = subsec();
    _x = (int64_t) sec * NSUBSEC + ss;
#else
    _sec = sec;
#endif
}

/** @brief Set this timestamp's subseconds component.
    @param subsec number of subseconds
    
    The seconds component is left unchanged. */
inline void
Timestamp::set_subsec(uint32_t subsec)
{
#if TIMESTAMP_PUNS_INT64
    uint32_t ss = this->subsec();
    _x = _x - ss + subsec;
#else
    _subsec = subsec;
#endif
}

/** @brief Set this timestamp to a seconds-and-microseconds value.
    @param sec number of seconds
    @param usec number of microseconds (must be less than 1000000)
    @sa make_usec() */
inline void
Timestamp::set_usec(seconds_type sec, uint32_t usec)
{
    assign(sec, usec_to_subsec(usec));
}

/** @brief Set this timestamp to a seconds-and-nanoseconds value.
    @param sec number of seconds
    @param nsec number of nanoseconds (must be less than 1000000000)
    @sa make_nsec() */
inline void
Timestamp::set_nsec(seconds_type sec, uint32_t nsec)
{
    assign(sec, nsec_to_subsec(nsec));
}

/** @brief Return this timestamp's seconds component. */
inline Timestamp::seconds_type
Timestamp::sec() const
{
#if TIMESTAMP_PUNS_INT64
    if (_x < 0)
	return (_x + 1) / NSUBSEC - 1;
    else
	return _x / NSUBSEC;
#else
    return _sec;
#endif
}

/** @brief Return this timestamp's subseconds component. */
inline uint32_t
Timestamp::subsec() const
{
#if TIMESTAMP_PUNS_INT64
    if (_x < 0)
	return NSUBSEC - 1 - (-_x - 1) % NSUBSEC;
    else
	return _x % NSUBSEC;
#else
    return _subsec;
#endif
}

/** @brief Return this timestamp's subseconds component, converted to
    milliseconds. */
inline uint32_t
Timestamp::msec() const
{
    return subsec_to_msec(subsec());
}

/** @brief Return this timestamp's subseconds component, converted to
    microseconds. */
inline uint32_t
Timestamp::usec() const
{
    return subsec_to_usec(subsec());
}

/** @brief Return this timestamp's subseconds component, converted to
    nanoseconds. */
inline uint32_t
Timestamp::nsec() const
{
    return subsec_to_nsec(subsec());
}

/** @brief Return this timestamp's interval length, converted to
    milliseconds.

    Will overflow on intervals of more than 2147483.647 seconds. */
inline Timestamp::seconds_type
Timestamp::msec1() const
{
#if TIMESTAMP_PUNS_INT64
    static_assert(NSUBSEC == 1000000000);
    return (_x + 500000) / 1000000;
#else
    return _sec * 1000 + subsec_to_msec(_subsec);
#endif
}

/** @brief Return this timestamp's interval length, converted to
    microseconds.

    Will overflow on intervals of more than 2147.483647 seconds. */
inline Timestamp::seconds_type
Timestamp::usec1() const
{
#if TIMESTAMP_PUNS_INT64
    static_assert(NSUBSEC == 1000000000);
    return (_x + 500) / 1000;
#else
    return _sec * 1000000 + subsec_to_usec(_subsec);
#endif
}

/** @brief Return this timestamp's interval length, converted to
    nanoseconds.

    Will overflow on intervals of more than 2.147483647 seconds. */
inline Timestamp::seconds_type
Timestamp::nsec1() const
{
#if TIMESTAMP_PUNS_INT64
    static_assert(NSUBSEC == 1000000000);
    return _x;
#else
    return _sec * 1000000000 + subsec_to_nsec(_subsec);
#endif
}

#if TIMESTAMP_PUNS_TIMEVAL
inline const struct timeval &
Timestamp::timeval() const
{
    return *(const struct timeval*) this;
}
#else
/** @brief Return a struct timeval with the same value as this timestamp.

    If Timestamp and struct timeval have the same size and representation,
    then this operation returns a "const struct timeval &" whose address is
    the same as this Timestamp. */
inline struct timeval
Timestamp::timeval() const
{
    struct timeval tv;
    tv.tv_sec = sec();
    tv.tv_usec = usec();
    return tv;
}
#endif

#if HAVE_STRUCT_TIMESPEC
# if TIMESTAMP_PUNS_TIMESPEC
inline const struct timespec &
Timestamp::timespec() const
{
    return *(const struct timespec*) this;
}
# else
/** @brief Return a struct timespec with the same value as this timestamp.

    If Timestamp and struct timespec have the same size and representation,
    then this operation returns a "const struct timespec &" whose address is
    the same as this Timestamp. */
inline struct timespec
Timestamp::timespec() const
{
    struct timespec tv;
    tv.tv_sec = sec();
    tv.tv_nsec = nsec();
    return tv;
}
# endif
#endif

#if !CLICK_TOOL
/** @brief Returns this timestamp, converted to a jiffies value. */ 
inline click_jiffies_t
Timestamp::jiffies() const
{
# if TIMESTAMP_PUNS_INT64
    // This is not very precise when CLICK_HZ doesn't divide NSUBSEC evenly.
    return value() / (NSUBSEC / CLICK_HZ);
# else
    click_jiffies_t j = ((click_jiffies_t) sec()) * CLICK_HZ;
#  if CLICK_HZ == 100 || CLICK_HZ == 1000 || CLICK_HZ == 10000 || CLICK_HZ == 100000 || CLICK_HZ == 1000000
    return j + ((click_jiffies_t) subsec()) / (NSUBSEC / CLICK_HZ);
#  else
    // This is not very precise when CLICK_HZ doesn't divide NSUBSEC evenly.
    return j + ((click_jiffies_t) subsec()) / (NSUBSEC / CLICK_HZ);
#  endif
# endif
}

inline Timestamp
Timestamp::make_jiffies(click_jiffies_t jiffies)
{
# if TIMESTAMP_PUNS_INT64
    // This is not very precise when CLICK_HZ doesn't divide NSUBSEC evenly.
    return Timestamp::make_value((int64_t) jiffies * (NSUBSEC / CLICK_HZ));
# else
#  if CLICK_HZ == 100 || CLICK_HZ == 1000 || CLICK_HZ == 10000 || CLICK_HZ == 100000 || CLICK_HZ == 1000000
    uint32_t subsec = (jiffies * (NSUBSEC / CLICK_HZ)) % NSUBSEC;
    return Timestamp(jiffies / (NSUBSEC / CLICK_HZ), subsec);
#  else
    // This is not very precise when CLICK_HZ doesn't divide NSUBSEC evenly.
    uint32_t subsec = (jiffies * (NSUBSEC / CLICK_HZ)) % NSUBSEC;
    return Timestamp(jiffies / (NSUBSEC / CLICK_HZ), subsec);
#  endif
# endif
}
#endif

/** @relates Timestamp
    @brief Compare two timestamps for equality.

    Returns true iff the two operands have the same seconds and subseconds
    components. */
inline bool
operator==(const Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_PUNS_INT64
    return a.value() == b.value();
#else
    return a.sec() == b.sec() && a.subsec() == b.subsec();
#endif
}

/** @relates Timestamp
    @brief Compare two timestamps for inequality.

    Returns true iff !(@a a == @a b). */
inline bool
operator!=(const Timestamp &a, const Timestamp &b)
{
    return !(a == b);
}

/** @relates Timestamp
    @brief Compare two timestamps.

    Returns true iff @a a represents a shorter interval than @a b, or
    considered as absolute time, @a a happened before @a b.  */
inline bool
operator<(const Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_PUNS_INT64
    return a.value() < b.value();
#else
    return a.sec() < b.sec() || (a.sec() == b.sec() && a.subsec() < b.subsec());
#endif
}

/** @relates Timestamp
    @brief Compare two timestamps.

    Returns true iff @a a measures an interval no larger than @a b, or
    considered as absolute time, @a a happened at or before @a b.  */
inline bool
operator<=(const Timestamp &a, const Timestamp &b)
{
    return !(b < a);
}

/** @relates Timestamp
    @brief Compare two timestamps.

    Returns true iff @a a measures an interval no shorter than @a b, or
    considered as absolute time, @a a happened at or after @a b.  */
inline bool
operator>=(const Timestamp &a, const Timestamp &b)
{
    return !(a < b);
}

/** @relates Timestamp
    @brief Compare two timestamps.

    Returns true iff @a a measures a longer interval than @a b, or considered
    as absolute time, @a a happened after @a b.  */
inline bool
operator>(const Timestamp &a, const Timestamp &b)
{
    return b < a;
}

#if !TIMESTAMP_PUNS_INT64
inline void
Timestamp::add_fix()
{
    if (_subsec >= NSUBSEC)
	_sec++, _subsec -= NSUBSEC;
}

inline void
Timestamp::sub_fix()
{
    if (_subsec < 0)
	_sec--, _subsec += NSUBSEC;
}
#endif

/** @brief Add @a b to @a a.

    Returns the result (the new value of @a a). */
inline Timestamp &
operator+=(Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_PUNS_INT64
    a._x += b._x;
#else
    a._sec += b._sec;
    a._subsec += b._subsec;
    a.add_fix();
#endif
    return a;
}

/** @brief Subtract @a b from @a a.

    Returns the result (the new value of @a a). */
inline Timestamp &
operator-=(Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_PUNS_INT64
    a._x -= b._x;
#else
    a._sec -= b._sec;
    a._subsec -= b._subsec;
    a.sub_fix();
#endif
    return a;
}

/** @brief Add the two operands and return the result. */
inline Timestamp
operator+(Timestamp a, const Timestamp &b)
{
    a += b;
    return a;
}

/** @brief Subtract @a b from @a a and return the result. */
inline Timestamp
operator-(Timestamp a, const Timestamp &b)
{
    a -= b;
    return a;
}

/** @brief Negate @a a and return the result. */
inline Timestamp
operator-(const Timestamp &a)
{
#if TIMESTAMP_PUNS_INT64
    return Timestamp::make_value(-a.value());
#else
    if (a.subsec())
	return Timestamp(-a.sec() - 1, Timestamp::NSUBSEC - a.subsec());
    else
	return Timestamp(-a.sec(), 0);
#endif
}

#if HAVE_FLOAT_TYPES
/** @brief Return this timestamp's value, converted to a real number. */
inline double
Timestamp::doubleval() const
{
# if TIMESTAMP_PUNS_INT64
    return _x / (double) NSUBSEC;
# else
    return _sec + (_subsec / (double) NSUBSEC);
# endif
}

/** @brief Create a timestamp measuring @a d seconds. */
inline
Timestamp::Timestamp(double d)
{
# if TIMESTAMP_PUNS_INT64
    _x = floor(d * NSUBSEC + 0.5);
# else
    double dfloor = floor(d);
    _sec = (seconds_type) dfloor;
    _subsec = (uint32_t) ((d - dfloor) * NSUBSEC + 0.5);
    add_fix();
# endif
}

/** @brief Scale @a a by a factor of @a b and return the result. */
inline Timestamp
operator*(const Timestamp &a, double b)
{
    return Timestamp(a.doubleval() * b);
}

inline Timestamp
operator*(const Timestamp &a, int b)
{
    return Timestamp(a.doubleval() * b);
}

inline Timestamp
operator*(const Timestamp &a, unsigned b)
{
    return Timestamp(a.doubleval() * b);
}

inline Timestamp
operator*(double a, const Timestamp &b)
{
    return Timestamp(b.doubleval() * a);
}

inline Timestamp
operator*(int a, const Timestamp &b)
{
    return Timestamp(b.doubleval() * a);
}

inline Timestamp
operator*(unsigned a, const Timestamp &b)
{
    return Timestamp(b.doubleval() * a);
}

/** @brief Scale @a a down by a factor of @a b and return the result. */
inline Timestamp
operator/(const Timestamp &a, double b)
{
    return Timestamp(a.doubleval() / b);
}

inline Timestamp
operator/(const Timestamp &a, int b)
{
    return Timestamp(a.doubleval() / b);
}

inline Timestamp
operator/(const Timestamp &a, unsigned b)
{
    return Timestamp(a.doubleval() / b);
}

/** @brief Divide @a a by @a b and return the result. */
inline double
operator/(const Timestamp &a, const Timestamp &b)
{
    return a.doubleval() / b.doubleval();
}
#endif /* HAVE_FLOAT_TYPES */

StringAccum& operator<<(StringAccum&, const Timestamp&);

CLICK_ENDDECLS
#undef TIMESTAMP_CONSTRUCTOR
#endif
