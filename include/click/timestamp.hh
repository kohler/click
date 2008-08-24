// -*- c-basic-offset: 4; related-file-name: "../../lib/timestamp.cc" -*-
#ifndef CLICK_TIMESTAMP_HH
#define CLICK_TIMESTAMP_HH
#include <click/glue.hh>
CLICK_DECLS
class String;

// Timestamp has three possible internal representations, selected by #defines.
// * TIMESTAMP_REP_FLAT64: a 64-bit integer number of nanoseconds
// * TIMESTAMP_REP_BIG_ENDIAN: 32 bits of seconds plus 32 bits of subseconds
// * TIMESTAMP_REP_LITTLE_ENDIAN: 32 bits of subseconds plus 32 bits of seconds
// 64-bit arithmetic is used when possible for speed.
// If Timestamp puns timespec/timeval, define TIMESTAMP_PUNS_TIMESPEC/TIMEVAL.

#if CLICK_LINUXMODULE
// The linuxmodule driver requires that Timestamp have the same representation
// as sk_buff's stamp/tstamp member.
# undef TIMESTAMP_REP_FLAT64
# undef TIMESTAMP_REP_BIG_ENDIAN
# undef TIMESTAMP_REP_LITTLE_ENDIAN
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#  define TIMESTAMP_REP_BIG_ENDIAN 1
# elif BITS_PER_LONG == 64 || defined(CONFIG_KTIME_SCALAR)
#  define TIMESTAMP_REP_FLAT64 1
# elif defined(__BIG_ENDIAN)
#  define TIMESTAMP_REP_BIG_ENDIAN 1
# else
#  define TIMESTAMP_REP_LITTLE_ENDIAN 1
# endif

#elif !TIMESTAMP_REP_FLAT64 && !TIMESTAMP_REP_BIG_ENDIAN && !TIMESTAMP_REP_LITTLE_ENDIAN
# if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
#  define TIMESTAMP_REP_BIG_ENDIAN 1
# else
#  define TIMESTAMP_REP_LITTLE_ENDIAN 1
# endif
#endif

#if HAVE_INT64_TYPES && !TIMESTAMP_REP_FLAT64 && ((TIMESTAMP_REP_BIG_ENDIAN && CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN) || (TIMESTAMP_REP_LITTLE_ENDIAN && CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN))
# define TIMESTAMP_MATH_FLAT64 1
#endif

#if TIMESTAMP_REP_BIG_ENDIAN && !HAVE_NANOTIMESTAMP && SIZEOF_STRUCT_TIMEVAL == 8
# define TIMESTAMP_PUNS_TIMEVAL 1
#elif TIMESTAMP_REP_BIG_ENDIAN && HAVE_NANOTIMESTAMP && HAVE_STRUCT_TIMESPEC && SIZEOF_STRUCT_TIMESPEC == 8
# define TIMESTAMP_PUNS_TIMESPEC 1
#endif

#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
# include <math.h>
#endif
#if (CLICK_USERLEVEL || CLICK_TOOL) && !CLICK_NS && HAVE_DECL_CLOCK_GETTIME && HAVE_CLOCK_GETTIME
# define CLICK_USE_CLOCK_GETTIME 1
#endif

#if TIMESTAMP_REP_FLAT64
# define TIMESTAMP_ASSIGN(s, ss) _t.x = (int64_t) (s) * subsec_per_sec + (ss)
#else
# define TIMESTAMP_ASSIGN(s, ss) _t.sec = (s), _t.subsec = (ss)
#endif

#if HAVE_NANOTIMESTAMP
# define PRITIMESTAMP "%d.%09d"
#else
# define PRITIMESTAMP "%d.%06d"
#endif

class Timestamp { public:

    /** @brief  Type represents a number of seconds. */
    typedef int32_t seconds_type;
    /** @brief  Return type for msecval(), usecval(), and nsecval(). */
#if HAVE_INT64_TYPES
    typedef int64_t value_type;
#else
    typedef int32_t value_type;
#endif

    enum {
	max_seconds = (seconds_type) 2147483647U,
	min_seconds = (seconds_type) -2147483648U,
	nsec_per_sec = 1000000000,
	nsec_per_msec = 1000000,
	nsec_per_usec = 1000,
	usec_per_sec = 1000000,
	usec_per_msec = 1000,
	msec_per_sec = 1000,
#if HAVE_NANOTIMESTAMP
	subsec_per_sec = nsec_per_sec
				/**< Number of subseconds in a second.  Can be
				     1000000 or 1000000000, depending on how
				     Click is compiled. */
#else
	subsec_per_sec = usec_per_sec
#endif
    };

    enum {
	NSUBSEC = subsec_per_sec
    };

    struct uninitialized_t {
    };


    /** @brief Construct a zero-valued Timestamp. */
    inline Timestamp() {
	TIMESTAMP_ASSIGN(0, 0);
    }

    /** @brief Construct a Timestamp of @a sec seconds plus @a subsec
     *     subseconds.
     * @param sec number of seconds
     * @param subsec number of subseconds (defaults to 0)
     *
     * The @a subsec parameter must be between 0 and subsec_per_sec - 1, and
     * the @a sec parameter must be between @link Timestamp::min_seconds
     * min_seconds @endlink and @link Timestamp::max_seconds max_seconds
     * @endlink.  Errors are not necessarily checked. */
    inline Timestamp(long sec, uint32_t subsec = 0) {
	TIMESTAMP_ASSIGN(sec, subsec);
    }
    /** @overload */
    inline Timestamp(int sec, uint32_t subsec = 0) {
	TIMESTAMP_ASSIGN(sec, subsec);
    }
    /** @overload */
    inline Timestamp(unsigned long sec, uint32_t subsec = 0) {
	TIMESTAMP_ASSIGN(sec, subsec);
    }
    /** @overload */
    inline Timestamp(unsigned sec, uint32_t subsec = 0) {
	TIMESTAMP_ASSIGN(sec, subsec);
    }
#if HAVE_FLOAT_TYPES
    inline Timestamp(double);
#endif

    inline Timestamp(const struct timeval &tv);
#if HAVE_STRUCT_TIMESPEC
    inline Timestamp(const struct timespec &ts);
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

    inline void set_sec(seconds_type sec);
    inline void set_subsec(uint32_t subsec);
    
    inline seconds_type msec1() const CLICK_DEPRECATED;
    inline seconds_type usec1() const CLICK_DEPRECATED;
    inline seconds_type nsec1() const CLICK_DEPRECATED;

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
    /** @brief Return this timestamp's interval length in milliseconds. */
    inline value_type msecval() const {
#if TIMESTAMP_REP_FLAT64
	return _t.x / (subsec_per_sec / msec_per_sec);
#else
	return (value_type) _t.sec * msec_per_sec + subsec_to_msec(_t.subsec);
#endif
    }
    /** @brief Return this timestamp's interval length in microseconds. */
    inline value_type usecval() const {
#if TIMESTAMP_REP_FLAT64
	return _t.x / (subsec_per_sec / usec_per_sec);
#else
	return (value_type) _t.sec * usec_per_sec + subsec_to_usec(_t.subsec);
#endif
    }
    /** @brief Return this timestamp's interval length in nanoseconds. */
    inline value_type nsecval() const {
#if TIMESTAMP_REP_FLAT64
	return _t.x * (nsec_per_sec / subsec_per_sec);
#else
	return (value_type) _t.sec * nsec_per_sec + subsec_to_nsec(_t.subsec);
#endif
    }

#if !CLICK_TOOL
    /** @brief Return a timestamp representing an interval of @a jiffies. */
    static inline Timestamp make_jiffies(click_jiffies_t jiffies);
    /** @brief Return the number of jiffies represented by this timestamp. */
    inline click_jiffies_t jiffies() const;
#endif

    static inline Timestamp make_sec(seconds_type sec);
    static inline Timestamp make_msec(value_type msec);
    static inline Timestamp make_usec(seconds_type sec, uint32_t usec);
    static inline Timestamp make_usec(value_type usec);
    static inline Timestamp make_nsec(seconds_type sec, uint32_t nsec);
    static inline Timestamp make_nsec(value_type nsec) {
	Timestamp t = Timestamp::uninitialized_t();
#if TIMESTAMP_REP_FLAT64
	t._t.x = nsec / (nsec_per_sec / subsec_per_sec);
#else
	// This arithmetic is about twice as fast on my laptop as the
	// alternative "t._t.sec = nsec / nsec_per_sec;
	//		nsec -= t._t.sec * nsec_per_sec;
	//		if (nsec < 0) t._t.sec--, nsec += nsec_per_sec;".
	if (unlikely(nsec < 0))
	    t._t.sec = -((-nsec - 1) / nsec_per_sec) - 1;
	else
	    t._t.sec = nsec / nsec_per_sec;
	nsec -= t._t.sec * nsec_per_sec;
	t._t.subsec = nsec / (nsec_per_sec / subsec_per_sec);
#endif
	return t;
    }

    static inline Timestamp now();
    /** @brief Return the smallest nonzero timestamp, Timestamp(0, 1). */
    static inline Timestamp epsilon() {
	return Timestamp(0, 1);
    }

    /** Assign this timestamp to a seconds-and-subseconds value.
     *
     * @sa Timestamp(int, int) */
    inline void assign(seconds_type sec, uint32_t subsec = 0);
    /** Assign this timestamp to a seconds-and-microseconds value. */
    inline void assign_usec(seconds_type sec, uint32_t usec) {
	assign(sec, usec_to_subsec(usec));
    }
    /** Assign this timestamp to a seconds-and-nanoseconds value. */
    inline void assign_nsec(seconds_type sec, uint32_t nsec) {
	assign(sec, nsec_to_subsec(nsec));
    }

    /** Assign this timestamp to a seconds-and-subseconds value.
     * @deprecated Use assign() instead. */
    inline void set(seconds_type sec, uint32_t subsec = 0) CLICK_DEPRECATED;
    /** Assign this timestamp to a seconds-and-microseconds value.
     * @deprecated Use assign_usec() instead. */
    inline void set_usec(seconds_type sec, uint32_t usec) CLICK_DEPRECATED;
    /** Assign this timestamp to a seconds-and-nanoseconds value.
     * @deprecated Use assign_nsec() instead. */
    inline void set_nsec(seconds_type sec, uint32_t nsec) CLICK_DEPRECATED;

    inline void set_now();
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
    int set_timeval_ioctl(int fd, int ioctl_selector);
#endif

    String unparse() const;

    /** @brief Convert milliseconds to subseconds.
     *
     * Subseconds are either microseconds or nanoseconds, depending on
     * configuration options and driver choice.
     * @sa usec_to_subsec(), nsec_to_subsec(), subsec_to_msec(),
     * subsec_to_usec(), subsec_to_nsec() */
    inline static uint32_t msec_to_subsec(uint32_t msec) {
	return msec * (subsec_per_sec / msec_per_sec);
    }
    /** @brief Convert microseconds to subseconds. */
    inline static uint32_t usec_to_subsec(uint32_t usec) {
	return usec * (subsec_per_sec / usec_per_sec);
    }
    /** @brief Convert nanoseconds to subseconds. */
    inline static uint32_t nsec_to_subsec(uint32_t nsec) {
	return nsec / (nsec_per_sec / subsec_per_sec);
    }
    /** @brief Convert subseconds to milliseconds. */
    inline static uint32_t subsec_to_msec(uint32_t subsec) {
	return subsec / (subsec_per_sec / msec_per_sec);
    }
    /** @brief Convert subseconds to microseconds. */
    inline static uint32_t subsec_to_usec(uint32_t subsec) {
	return subsec / (subsec_per_sec / usec_per_sec);
    }
    /** @brief Convert subseconds to nanoseconds. */
    inline static uint32_t subsec_to_nsec(uint32_t subsec) {
	return subsec * (nsec_per_sec / subsec_per_sec);
    }

  private:

    union {
#if TIMESTAMP_REP_FLAT64 || TIMESTAMP_MATH_FLAT64
	int64_t x;
#endif
#if TIMESTAMP_REP_BIG_ENDIAN
	struct {
	    int32_t sec;
	    int32_t subsec;
	};
#elif TIMESTAMP_REP_LITTLE_ENDIAN
	struct {
	    int32_t subsec;
	    int32_t sec;
	};
#endif
    } _t;

#if !TIMESTAMP_REP_FLAT64
    inline void add_fix();
    inline void sub_fix();
#endif

    friend inline bool operator==(const Timestamp &a, const Timestamp &b);
    friend inline bool operator<(const Timestamp &a, const Timestamp &b);
    friend inline Timestamp operator-(const Timestamp &b);
    friend inline Timestamp &operator+=(Timestamp &a, const Timestamp &b);
    friend inline Timestamp &operator-=(Timestamp &a, const Timestamp &b);

};


/** @brief Create a Timestamp measuring @a tv.
    @param tv timeval structure */
inline
Timestamp::Timestamp(const struct timeval& tv)
{
    TIMESTAMP_ASSIGN(tv.tv_sec, usec_to_subsec(tv.tv_usec));
}

#if HAVE_STRUCT_TIMESPEC
/** @brief Create a Timestamp measuring @a ts.
    @param ts timespec structure */
inline
Timestamp::Timestamp(const struct timespec& ts)
{
    TIMESTAMP_ASSIGN(ts.tv_sec, nsec_to_subsec(ts.tv_nsec));
}
#endif

/** @brief Return true iff this timestamp is not zero-valued. */
inline
Timestamp::operator unspecified_bool_type() const
{
#if TIMESTAMP_REP_FLAT64 || TIMESTAMP_MATH_FLAT64
    return _t.x ? &Timestamp::sec : 0;
#else
    return _t.sec || _t.subsec ? &Timestamp::sec : 0;
#endif
}

/** @brief Set this timestamp's components.
    @param sec number of seconds
    @param subsec number of subseconds */
inline void
Timestamp::assign(seconds_type sec, uint32_t subsec)
{
    TIMESTAMP_ASSIGN(sec, subsec);
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
    @param msec number of milliseconds (may be negative or greater than 1000) */
inline Timestamp
Timestamp::make_msec(value_type msec)
{
    Timestamp t = Timestamp::uninitialized_t();
#if TIMESTAMP_REP_FLAT64
    t._t.x = msec * (subsec_per_sec / msec_per_sec);
#else
    if (unlikely(msec < 0))
	t._t.sec = -((-msec - 1) / msec_per_sec) - 1;
    else
	t._t.sec = msec / msec_per_sec;
    msec -= t._t.sec * msec_per_sec;
    t._t.subsec = msec * (subsec_per_sec / msec_per_sec);
#endif
    return t;
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
    @param usec number of microseconds (may be negative or greater than 1000000) */
inline Timestamp
Timestamp::make_usec(value_type usec)
{
    Timestamp t = Timestamp::uninitialized_t();
#if TIMESTAMP_REP_FLAT64
    t._t.x = usec * (subsec_per_sec / usec_per_sec);
#else
    if (unlikely(usec < 0))
	t._t.sec = -((-usec - 1) / usec_per_sec) - 1;
    else
	t._t.sec = usec / usec_per_sec;
    usec -= t._t.sec * usec_per_sec;
    t._t.subsec = usec * (subsec_per_sec / usec_per_sec);
#endif
    return t;
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
#if TIMESTAMP_REP_FLAT64
    uint32_t ss = subsec();
    _t.x = (int64_t) sec * subsec_per_sec + ss;
#else
    _t.sec = sec;
#endif
}

/** @brief Set this timestamp's subseconds component.
    @param subsec number of subseconds
    
    The seconds component is left unchanged. */
inline void
Timestamp::set_subsec(uint32_t subsec)
{
#if TIMESTAMP_REP_FLAT64
    uint32_t ss = this->subsec();
    _t.x = _t.x - ss + subsec;
#else
    _t.subsec = subsec;
#endif
}

/** @brief Return this timestamp's seconds component. */
inline Timestamp::seconds_type
Timestamp::sec() const
{
#if TIMESTAMP_REP_FLAT64
    if (_t.x < 0)
	return (_t.x + 1) / subsec_per_sec - 1;
    else
	return _t.x / subsec_per_sec;
#else
    return _t.sec;
#endif
}

/** @brief Return this timestamp's subseconds component. */
inline uint32_t
Timestamp::subsec() const
{
#if TIMESTAMP_REP_FLAT64
    if (_t.x < 0)
	return subsec_per_sec - 1 - (-_t.x - 1) % subsec_per_sec;
    else
	return _t.x % subsec_per_sec;
#else
    return _t.subsec;
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
#if TIMESTAMP_REP_FLAT64
    return _t.x / (subsec_per_sec / msec_per_sec);
#else
    return _t.sec * msec_per_sec + subsec_to_msec(_t.subsec);
#endif
}

/** @brief Return this timestamp's interval length, converted to
    microseconds.

    Will overflow on intervals of more than 2147.483647 seconds. */
inline Timestamp::seconds_type
Timestamp::usec1() const
{
#if TIMESTAMP_REP_FLAT64
    return _t.x / (subsec_per_sec / usec_per_sec);
#else
    return _t.sec * usec_per_sec + subsec_to_usec(_t.subsec);
#endif
}

/** @brief Return this timestamp's interval length, converted to
    nanoseconds.

    Will overflow on intervals of more than 2.147483647 seconds. */
inline Timestamp::seconds_type
Timestamp::nsec1() const
{
#if TIMESTAMP_REP_FLAT64
    return _t.x * (nsec_per_sec / subsec_per_sec);
#else
    return _t.sec * nsec_per_sec + subsec_to_nsec(_t.subsec);
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
# if TIMESTAMP_REP_FLAT64
    // This is not very precise when CLICK_HZ doesn't divide NSUBSEC evenly.
    return _t.x / (subsec_per_sec / CLICK_HZ);
# else
    click_jiffies_t j = ((click_jiffies_t) sec()) * CLICK_HZ;
#  if CLICK_HZ == 100 || CLICK_HZ == 1000 || CLICK_HZ == 10000 || CLICK_HZ == 100000 || CLICK_HZ == 1000000
    return j + ((click_jiffies_t) subsec()) / (subsec_per_sec / CLICK_HZ);
#  else
    // This is not very precise when CLICK_HZ doesn't divide NSUBSEC evenly.
    return j + ((click_jiffies_t) subsec()) / (subsec_per_sec / CLICK_HZ);
#  endif
# endif
}

inline Timestamp
Timestamp::make_jiffies(click_jiffies_t jiffies)
{
# if TIMESTAMP_REP_FLAT64
    // Not very precise when CLICK_HZ doesn't evenly divide subsec_per_sec.
    Timestamp t = Timestamp::uninitialized_t();
    t._t.x = (int64_t) jiffies * (subsec_per_sec / CLICK_HZ);
    return t;
# else
#  if CLICK_HZ == 100 || CLICK_HZ == 1000 || CLICK_HZ == 10000 || CLICK_HZ == 100000 || CLICK_HZ == 1000000
    uint32_t subsec = (jiffies * (subsec_per_sec / CLICK_HZ)) % subsec_per_sec;
    return Timestamp(jiffies / (subsec_per_sec / CLICK_HZ), subsec);
#  else
    // Not very precise when CLICK_HZ doesn't evenly divide subsec_per_sec.
    uint32_t subsec = (jiffies * (subsec_per_sec / CLICK_HZ)) % subsec_per_sec;
    return Timestamp(jiffies / (subsec_per_sec / CLICK_HZ), subsec);
#  endif
# endif
}
#endif

inline void Timestamp::set(seconds_type sec, uint32_t subsec) {
    assign(sec, subsec);
}

inline void Timestamp::set_usec(seconds_type sec, uint32_t usec) {
    assign_usec(sec, usec);
}

inline void Timestamp::set_nsec(seconds_type sec, uint32_t nsec) {
    assign_nsec(sec, nsec);
}

/** @relates Timestamp
    @brief Compare two timestamps for equality.

    Returns true iff the two operands have the same seconds and subseconds
    components. */
inline bool
operator==(const Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_REP_FLAT64 || TIMESTAMP_MATH_FLAT64
    return a._t.x == b._t.x;
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
#if TIMESTAMP_REP_FLAT64 || TIMESTAMP_MATH_FLAT64
    return a._t.x < b._t.x;
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

#if !TIMESTAMP_REP_FLAT64
inline void
Timestamp::add_fix()
{
# if TIMESTAMP_MATH_FLAT64
    if (_t.subsec >= subsec_per_sec)
	_t.x += (uint32_t) -subsec_per_sec;
# else
    if (_t.subsec >= subsec_per_sec)
	_t.sec++, _t.subsec -= subsec_per_sec;
# endif
}

inline void
Timestamp::sub_fix()
{
# if TIMESTAMP_MATH_FLAT64
    if (_t.subsec < 0)
	_t.subsec += subsec_per_sec;
# else
    if (_t.subsec < 0)
	_t.sec--, _t.subsec += subsec_per_sec;
# endif
}
#endif

/** @brief Add @a b to @a a.

    Returns the result (the new value of @a a). */
inline Timestamp &
operator+=(Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_REP_FLAT64
    a._t.x += b._t.x;
#elif TIMESTAMP_MATH_FLAT64
    a._t.x += b._t.x;
    a.add_fix();
#else
    a._t.sec += b._t.sec;
    a._t.subsec += b._t.subsec;
    a.add_fix();
#endif
    return a;
}

/** @brief Subtract @a b from @a a.

    Returns the result (the new value of @a a). */
inline Timestamp &
operator-=(Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_REP_FLAT64
    a._t.x -= b._t.x;
#elif TIMESTAMP_MATH_FLAT64
    a._t.x -= b._t.x;
    a.sub_fix();
#else
    a._t.sec -= b._t.sec;
    a._t.subsec -= b._t.subsec;
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
#if TIMESTAMP_REP_FLAT64
    Timestamp t = Timestamp::uninitialized_t();
    t._t.x = -a._t.x;
    return t;
#else
    if (a.subsec())
	return Timestamp(-a.sec() - 1, Timestamp::subsec_per_sec - a.subsec());
    else
	return Timestamp(-a.sec(), 0);
#endif
}

#if HAVE_FLOAT_TYPES
/** @brief Return this timestamp's value, converted to a real number. */
inline double
Timestamp::doubleval() const
{
# if TIMESTAMP_REP_FLAT64
    return _t.x / (double) subsec_per_sec;
# else
    return _t.sec + (_t.subsec / (double) subsec_per_sec);
# endif
}

/** @brief Create a timestamp measuring @a d seconds. */
inline
Timestamp::Timestamp(double d)
{
# if TIMESTAMP_REP_FLAT64
    _t.x = floor(d * subsec_per_sec + 0.5);
# else
    double dfloor = floor(d);
    _t.sec = (seconds_type) dfloor;
    _t.subsec = (uint32_t) ((d - dfloor) * subsec_per_sec + 0.5);
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
#undef TIMESTAMP_ASSIGN
#endif
