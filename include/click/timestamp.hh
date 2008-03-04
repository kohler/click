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

#if HAVE_NANOTIMESTAMP
# define PRITIMESTAMP "%d.%09d"
#else
# define PRITIMESTAMP "%d.%06d"
#endif

class Timestamp { public:

    /** @brief  Type used to represent a number of seconds. */
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

    inline Timestamp();
    inline Timestamp(seconds_type sec, uint32_t subsec);
    inline Timestamp(const struct timeval &tv);
#if HAVE_STRUCT_TIMESPEC
    inline Timestamp(const struct timespec &ts);
#endif
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
    inline Timestamp(double);
#endif

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
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
    inline double doubleval() const;
#endif

    static inline Timestamp make_sec(seconds_type sec);
    static inline Timestamp make_msec(uint32_t msec);
    static inline Timestamp make_usec(seconds_type sec, uint32_t usec);
    static inline Timestamp make_usec(uint32_t usec);
    static inline Timestamp make_nsec(seconds_type sec, uint32_t nsec);
    
    static inline Timestamp epsilon();
    static inline Timestamp now();

    inline void set(seconds_type sec, uint32_t subsec);
    inline void set_sec(seconds_type sec);
    inline void set_subsec(uint32_t subsec);
    inline void set_usec(seconds_type sec, uint32_t usec);
    inline void set_nsec(seconds_type sec, uint32_t nsec);

    inline void set_now();
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
    int set_timeval_ioctl(int fd, int ioctl_selector);
#endif

    void add_fix();
    void sub_fix();

    String unparse() const;

    inline static uint32_t msec_to_subsec(uint32_t msec);
    inline static uint32_t usec_to_subsec(uint32_t usec);
    inline static uint32_t nsec_to_subsec(uint32_t nsec);
    inline static uint32_t subsec_to_msec(uint32_t subsec);
    inline static uint32_t subsec_to_usec(uint32_t subsec);
    inline static uint32_t subsec_to_nsec(uint32_t subsec);
    
    seconds_type _sec;
    int32_t _subsec;
    
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
    return subsec / 1000000;
#else
    return subsec / 1000;
#endif
}

/** @brief Convert subseconds to microseconds.
 @param subsec number of subseconds
 @sa msec_to_subsec() */
inline uint32_t
Timestamp::subsec_to_usec(uint32_t subsec)
{
#if HAVE_NANOTIMESTAMP
    return subsec / 1000;
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


/** @brief Create a zero-valued Timestamp. */
inline
Timestamp::Timestamp()
    : _sec(0), _subsec(0)
{
}

/** @brief Create a Timestamp measuring @a sec seconds plus @a subsec subseconds.
    @param sec number of seconds
    @param subsec number of subseconds
 
    The @a subsec parameter must be between 0 and NSUBSEC - 1; errors are not
    necessarily checked. */
inline
Timestamp::Timestamp(seconds_type sec, uint32_t subsec)
    : _sec(sec), _subsec(subsec)
{
}

/** @brief Create a Timestamp measuring @a tv.
    @param tv timeval structure */
inline
Timestamp::Timestamp(const struct timeval& tv)
    : _sec(tv.tv_sec), _subsec(usec_to_subsec(tv.tv_usec))
{
}

#if HAVE_STRUCT_TIMESPEC
/** @brief Create a Timestamp measuring @a ts.
    @param ts timespec structure */
inline
Timestamp::Timestamp(const struct timespec& ts)
    : _sec(ts.tv_sec), _subsec(nsec_to_subsec(ts.tv_nsec))
{
}
#endif

/** @brief Returns true iff this timestamp is not zero-valued. */
inline
Timestamp::operator unspecified_bool_type() const
{
    return _sec || _subsec ? &Timestamp::sec : 0;
}

/** @brief Sets this timestamp to the current time.

 The current time is measured in seconds since January 1, 1970 GMT.
 @sa now() */
inline void
Timestamp::set_now()
{
#if TIMESTAMP_PUNS_TIMEVAL
    click_gettimeofday((struct timeval*) this);
#else
    struct timeval tv;
    click_gettimeofday(&tv);
    _sec = tv.tv_sec;
    _subsec = usec_to_subsec(tv.tv_usec);
#endif
}

/** @brief Return a timestamp representing the current time.

 The current time is measured in seconds since January 1, 1970 GMT.
 @sa set_now() */
inline Timestamp
Timestamp::now()
{
    Timestamp t;
    t.set_now();
    return t;
}

/** @brief Returns the smallest nonzero timestamp.

 Same as Timestamp(0, 1). */
inline Timestamp
Timestamp::epsilon()
{
    return Timestamp(0, 1);
}

/** @brief Returns a timestamp representing an interval of @a sec
    seconds.
    @param sec number of seconds */
inline Timestamp
Timestamp::make_sec(seconds_type sec)
{
    return Timestamp(sec, 0);
}

/** @brief Returns a timestamp representing an interval of @a msec
    milliseconds.
    @param msec number of milliseconds (may be greater than 1000) */
inline Timestamp
Timestamp::make_msec(uint32_t msec)
{
    return Timestamp(msec / 1000, msec_to_subsec(msec % 1000));
}

/** @brief Returns a timestamp representing @a sec seconds plus @a usec
    microseconds.
    @param sec number of seconds
    @param usec number of microseconds (less than 1000000) */
inline Timestamp
Timestamp::make_usec(seconds_type sec, uint32_t usec)
{
    return Timestamp(sec, usec_to_subsec(usec));
}

/** @brief Returns a timestamp representing an interval of @a usec
    microseconds.
    @param usec number of microseconds (may be greater than 1000000) */
inline Timestamp
Timestamp::make_usec(uint32_t usec)
{
    return Timestamp(usec / 1000000, usec_to_subsec(usec % 1000000));
}

/** @brief Returns a timestamp representing @a sec seconds plus @a nsec
    nanoseconds.
    @param sec number of seconds
    @param nsec number of nanoseconds (less than 1000000000) */
inline Timestamp
Timestamp::make_nsec(seconds_type sec, uint32_t nsec)
{
    return Timestamp(sec, nsec_to_subsec(nsec));
}

/** @brief Sets this timestamp's components.
    @param sec number of seconds
    @param subsec number of subseconds */
inline void
Timestamp::set(seconds_type sec, uint32_t subsec)
{
    _sec = sec;
    _subsec = subsec;
}

/** @brief Sets this timestamp's seconds component.
    @param sec number of seconds
    
    The subseconds component is left unchanged. */
inline void
Timestamp::set_sec(seconds_type sec)
{
    _sec = sec;
}

/** @brief Sets this timestamp's subseconds component.
    @param subsec number of subseconds
    
    The seconds component is left unchanged. */
inline void
Timestamp::set_subsec(uint32_t subsec)
{
    _subsec = subsec;
}

/** @brief Sets this timestamp to a seconds-and-microseconds value.
    @param sec number of seconds
    @param usec number of microseconds (must be less than 1000000)
    @sa make_usec() */
inline void
Timestamp::set_usec(seconds_type sec, uint32_t usec)
{
    _sec = sec;
    _subsec = usec_to_subsec(usec);
}

/** @brief Sets this timestamp to a seconds-and-nanoseconds value.
    @param sec number of seconds
    @param nsec number of nanoseconds (must be less than 1000000000)
    @sa make_nsec() */
inline void
Timestamp::set_nsec(seconds_type sec, uint32_t nsec)
{
    _sec = sec;
    _subsec = nsec_to_subsec(nsec);
}

/** @brief Returns this timestamp's seconds component. */
inline Timestamp::seconds_type
Timestamp::sec() const
{
    return _sec;
}

/** @brief Returns this timestamp's subseconds component. */
inline uint32_t
Timestamp::subsec() const
{
    return _subsec;
}

/** @brief Returns this timestamp's subseconds component, converted to
    milliseconds. */
inline uint32_t
Timestamp::msec() const
{
    return subsec_to_msec(_subsec);
}

/** @brief Returns this timestamp's subseconds component, converted to
    microseconds. */
inline uint32_t
Timestamp::usec() const
{
    return subsec_to_usec(_subsec);
}

/** @brief Returns this timestamp's subseconds component, converted to
    nanoseconds. */
inline uint32_t
Timestamp::nsec() const
{
    return subsec_to_nsec(_subsec);
}

/** @brief Returns this timestamp's interval length, converted to
    milliseconds.

    Will overflow on intervals of more than 2147483.647 seconds. */
inline Timestamp::seconds_type
Timestamp::msec1() const
{
    return _sec * 1000 + subsec_to_msec(_subsec);
}

/** @brief Returns this timestamp's interval length, converted to
    microseconds.

    Will overflow on intervals of more than 2147.483647 seconds. */
inline Timestamp::seconds_type
Timestamp::usec1() const
{
    return _sec * 1000000 + subsec_to_usec(_subsec);
}

/** @brief Returns this timestamp's interval length, converted to
    nanoseconds.

    Will overflow on intervals of more than 2.147483647 seconds. */
inline Timestamp::seconds_type
Timestamp::nsec1() const
{
    return _sec * 1000000000 + subsec_to_nsec(_subsec);
}

#if TIMESTAMP_PUNS_TIMEVAL
inline const struct timeval &
Timestamp::timeval() const
{
    return *(const struct timeval*) this;
}
#else
/** @brief Returns a struct timeval with the same value as this timestamp.

    If Timestamp and struct timeval have the same size and representation,
    then this operation returns a "const struct timeval &" whose address is
    the same as this Timestamp. */
inline struct timeval
Timestamp::timeval() const
{
    struct timeval tv;
    tv.tv_sec = _sec;
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
/** @brief Returns a struct timespec with the same value as this timestamp.

    If Timestamp and struct timespec have the same size and representation,
    then this operation returns a "const struct timespec &" whose address is
    the same as this Timestamp. */
inline struct timespec
Timestamp::timespec() const
{
    struct timespec tv;
    tv.tv_sec = _sec;
    tv.tv_nsec = nsec();
    return tv;
}
# endif
#endif

/** @relates Timestamp
    @brief Compares two timestamps for equality.

    Returns true iff the two operands have the same seconds and subseconds
    components. */
inline bool
operator==(const Timestamp &a, const Timestamp &b)
{
    return a.sec() == b.sec() && a.subsec() == b.subsec();
}

/** @relates Timestamp
    @brief Compares two timestamps for inequality.

    Returns true iff !(@a a == @a b). */
inline bool
operator!=(const Timestamp &a, const Timestamp &b)
{
    return !(a == b);
}

/** @relates Timestamp
    @brief Compares two timestamps.

    Returns true iff @a a represents a shorter interval than @a b, or
    considered as absolute time, @a a happened before @a b.  */
inline bool
operator<(const Timestamp &a, const Timestamp &b)
{
    return a.sec() < b.sec() || (a.sec() == b.sec() && a.subsec() < b.subsec());
}

/** @relates Timestamp
    @brief Compares two timestamps.

    Returns true iff @a a measures an interval no larger than @a b, or
    considered as absolute time, @a a happened at or before @a b.  */
inline bool
operator<=(const Timestamp &a, const Timestamp &b)
{
    return !(b < a);
}

/** @relates Timestamp
    @brief Compares two timestamps.

    Returns true iff @a a measures an interval no shorter than @a b, or
    considered as absolute time, @a a happened at or after @a b.  */
inline bool
operator>=(const Timestamp &a, const Timestamp &b)
{
    return !(a < b);
}

/** @relates Timestamp
    @brief Compares two timestamps.

    Returns true iff @a a measures a longer interval than @a b, or considered
    as absolute time, @a a happened after @a b.  */
inline bool
operator>(const Timestamp &a, const Timestamp &b)
{
    return b < a;
}

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

/** @brief Adds @a b to @a a.

    Returns the result (the new value of @a a). */
inline Timestamp &
operator+=(Timestamp &a, const Timestamp &b)
{
    a._sec += b._sec;
    a._subsec += b._subsec;
    a.add_fix();
    return a;
}

/** @brief Subtracts @a b from @a a.

    Returns the result (the new value of @a a). */
inline Timestamp &
operator-=(Timestamp &a, const Timestamp &b)
{
    a._sec -= b._sec;
    a._subsec -= b._subsec;
    a.sub_fix();
    return a;
}

/** @brief Adds the two operands and returns the result. */
inline Timestamp
operator+(Timestamp a, const Timestamp &b)
{
    a += b;
    return a;
}

/** @brief Subtracts @a b from @a a and returns the result. */
inline Timestamp
operator-(Timestamp a, const Timestamp &b)
{
    a -= b;
    return a;
}

/** @brief Negates @a a and returns the result. */
inline Timestamp
operator-(const Timestamp &a)
{
    if (a._subsec)
	return Timestamp(-a._sec - 1, Timestamp::NSUBSEC - a._subsec);
    else
	return Timestamp(-a._sec, 0);
}

#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
/** @brief Returns this timestamp's value, converted to a real number. */
inline double
Timestamp::doubleval() const
{
    return _sec + (_subsec / (double) NSUBSEC);
}

/** @brief Create a timestamp measuring @a d seconds. */
inline
Timestamp::Timestamp(double d)
{
    double dfloor = floor(d);
    _sec = (seconds_type) dfloor;
    _subsec = (uint32_t) ((d - dfloor) * NSUBSEC + 0.5);
    add_fix();
}

/** @brief Scales @a a by a factor of @a b and returns the result. */
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

/** @brief Scales @a a down by a factor of @a b and returns the result. */
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

/** @brief Divides @a a by @a b and returns the result. */
inline double
operator/(const Timestamp &a, const Timestamp &b)
{
    return a.doubleval() / b.doubleval();
}
#endif /* !CLICK_LINUXMODULE && !CLICK_BSDMODULE */

StringAccum& operator<<(StringAccum&, const Timestamp&);

CLICK_ENDDECLS
#endif
