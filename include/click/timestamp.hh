// -*- c-basic-offset: 4; related-file-name: "../../lib/timestamp.cc" -*-
#ifndef CLICK_TIMESTAMP_HH
#define CLICK_TIMESTAMP_HH
#include <click/glue.hh>
#include <click/type_traits.hh>
#include <click/integers.hh>
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
# include <math.h>
#endif
CLICK_DECLS
class String;
class Timestamp;
Timestamp operator+(Timestamp, const Timestamp &);

// Timestamp has three possible internal representations, selected by #defines.
// * TIMESTAMP_REP_FLAT64: a 64-bit integer number of nanoseconds
// * TIMESTAMP_REP_BIG_ENDIAN: 32 bits of seconds plus 32 bits of subseconds
// * TIMESTAMP_REP_LITTLE_ENDIAN: 32 bits of subseconds plus 32 bits of seconds
//
// Rationale: The linuxmodule driver must select the same representation as
// Linux's sk_buff tstamp member, which may use any of these representations.
// (Linuxmodule Packet objects are equivalent to sk_buffs, and
// Packet::timestamp_anno() maps to sk_buff::tstamp.  We want to avoid
// conversion expense when accessing timestamp_anno().  More seriously, it is
// very convenient to treat timestamp_anno() as a modifiable reference.)

#if !TIMESTAMP_REP_FLAT64 && !TIMESTAMP_REP_BIG_ENDIAN && !TIMESTAMP_REP_LITTLE_ENDIAN
# if CLICK_LINUXMODULE
#  if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#   define TIMESTAMP_REP_BIG_ENDIAN 1
#  elif BITS_PER_LONG == 64 || defined(CONFIG_KTIME_SCALAR)
#   define TIMESTAMP_REP_FLAT64 1
#  elif defined(__BIG_ENDIAN)
#   define TIMESTAMP_REP_BIG_ENDIAN 1
#  else
#   define TIMESTAMP_REP_LITTLE_ENDIAN 1
#  endif
# elif HAVE_INT64_TYPES && SIZEOF_LONG == 8
#  define TIMESTAMP_REP_FLAT64 1
# elif HAVE_INT64_TYPES && CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
#  define TIMESTAMP_REP_LITTLE_ENDIAN 1
# else
#  define TIMESTAMP_REP_BIG_ENDIAN 1
# endif
#endif


// Timestamp can use microsecond or nanosecond precision.  Nanosecond
// precision is used if TIMESTAMP_NANOSEC == 1.  In the Linux kernel, we
// choose what Linux uses for sk_buff::tstamp.  Elsewhere, we default to
// microsecond precision (XXX); "./configure --enable-nanotimestamp" selects
// nanosecond precision.

#ifndef TIMESTAMP_NANOSEC
# if CLICK_LINUXMODULE
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#   define TIMESTAMP_NANOSEC 1
#  endif
# endif
#endif


// Define TIMESTAMP_MATH_FLAT64 if despite a seconds-and-subseconds
// representation, 64-bit arithmetic should be used for timestamp addition,
// subtraction, and comparisons.  This can be faster than operating on two
// separate 32-bit integers.

#if HAVE_INT64_TYPES && !TIMESTAMP_REP_FLAT64
# if (TIMESTAMP_REP_BIG_ENDIAN && CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN) \
    || (TIMESTAMP_REP_LITTLE_ENDIAN && CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN)
#  define TIMESTAMP_MATH_FLAT64 1
# endif
#endif


// If a Timestamp's internal representation is identical to struct timespec
// or struct timeval, define TIMESTAMP_PUNS_TIMESPEC or TIMESTAMP_PUNS_TIMEVAL.

#if TIMESTAMP_REP_BIG_ENDIAN
# if !TIMESTAMP_NANOSEC && SIZEOF_STRUCT_TIMEVAL == 8
#  define TIMESTAMP_PUNS_TIMEVAL 1
# elif TIMESTAMP_NANOSEC && HAVE_STRUCT_TIMESPEC && SIZEOF_STRUCT_TIMESPEC == 8
#  define TIMESTAMP_PUNS_TIMESPEC 1
# endif
#endif


// Timestamp::value_type is the type of arguments to, for example,
// Timestamp::make_msec(), and should be as large as possible.  This is
// int64_t at userlevel.  In the linuxmodule driver, int64_t can only be used
// if the relevant Linux provides 64-bit divide, which is do_div.
// TIMESTAMP_VALUE_INT64 is defined to 1 if Timestamp::value_type is int64_t.

#if !defined(TIMESTAMP_VALUE_INT64) && HAVE_INT64_TYPES
# if CLICK_LINUXMODULE && defined(do_div)
#  define TIMESTAMP_VALUE_INT64 1
# elif !CLICK_LINUXMODULE && !CLICK_BSDMODULE
#  define TIMESTAMP_VALUE_INT64 1
# endif
#endif


// PRITIMESTAMP is a printf format string for Timestamps.  The corresponding
// printf argument list is Timestamp::sec() and Timestamp::subsec(), in that
// order.

#if TIMESTAMP_NANOSEC
# define PRITIMESTAMP "%d.%09d"
#else
# define PRITIMESTAMP "%d.%06d"
#endif


// TIMESTAMP_WARPABLE is defined if this Timestamp implementation supports
// timewarping.

#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE && !CLICK_NS
# define TIMESTAMP_WARPABLE 1
#endif


class Timestamp { public:

    /** @brief  Type represents a number of seconds. */
    typedef int32_t seconds_type;
    /** @brief  Return type for msecval(), usecval(), and nsecval(). */
#if TIMESTAMP_VALUE_INT64
    typedef int64_t value_type;
#else
    typedef int32_t value_type;
#endif

    enum {
        max_seconds = (seconds_type) 2147483647U,
                        /**< Maximum number of seconds representable in a
                             Timestamp. */
        min_seconds = (seconds_type) -2147483648U
                        /**< Minimum number of seconds representable in a
                             Timestamp. */
    };

    enum {
        nsec_per_sec = 1000000000,
        nsec_per_msec = 1000000,
        nsec_per_usec = 1000,
        usec_per_sec = 1000000,
        usec_per_msec = 1000,
        msec_per_sec = 1000,
#if TIMESTAMP_NANOSEC
        subsec_per_sec = nsec_per_sec,
                                /**< Number of subseconds in a second.  Can be
                                     1000000 or 1000000000, depending on how
                                     Click is compiled. */
#else
        subsec_per_sec = usec_per_sec,
#endif
        subsec_per_msec = subsec_per_sec / msec_per_sec,
        subsec_per_usec = subsec_per_sec / usec_per_sec
#if CLICK_NS
        , schedule_granularity = usec_per_sec
#endif
    };

    enum {
        NSUBSEC = subsec_per_sec
    };

    typedef uninitialized_type uninitialized_t;

    union rep_t;


    /** @brief Construct a zero-valued Timestamp. */
    inline Timestamp() {
        assign(0, 0);
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
    explicit inline Timestamp(long sec, uint32_t subsec = 0) {
        assign(sec, subsec);
    }
    /** @overload */
    explicit inline Timestamp(int sec, uint32_t subsec = 0) {
        assign(sec, subsec);
    }
    /** @overload */
    explicit inline Timestamp(unsigned long sec, uint32_t subsec = 0) {
        assign(sec, subsec);
    }
    /** @overload */
    explicit inline Timestamp(unsigned sec, uint32_t subsec = 0) {
        assign(sec, subsec);
    }
#if HAVE_FLOAT_TYPES
    explicit inline Timestamp(double);
#endif

    inline Timestamp(const struct timeval &tv);
#if HAVE_STRUCT_TIMESPEC
    inline Timestamp(const struct timespec &ts);
#endif

    /** @brief Construct a Timestamp from its internal representation. */
    inline Timestamp(const rep_t &rep)
        : _t(rep) {
    }

    /** @brief Construct an uninitialized timestamp. */
    inline Timestamp(const uninitialized_t &unused) {
        (void) unused;
    }

    typedef seconds_type (Timestamp::*unspecified_bool_type)() const;
    inline operator unspecified_bool_type() const;

    /** @brief Test if this Timestamp is negative (< Timestamp(0, 0)). */
    inline bool is_negative() const {
#if TIMESTAMP_REP_FLAT64 || TIMESTAMP_MATH_FLAT64
        return _t.x < 0;
#else
        return sec() < 0;
#endif
    }

    seconds_type sec() const;
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
    inline const struct timeval &timeval_ceil() const;
#else
    inline struct timeval timeval() const;
    inline struct timeval timeval_ceil() const;
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
        return value_div(_t.x, subsec_per_sec / msec_per_sec);
#else
        return (value_type) _t.sec * msec_per_sec + subsec_to_msec(_t.subsec);
#endif
    }
    /** @brief Return this timestamp's interval length in microseconds. */
    inline value_type usecval() const {
#if TIMESTAMP_REP_FLAT64
        return value_div(_t.x, subsec_per_sec / usec_per_sec);
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

    /** @brief Return the next millisecond-valued timestamp no smaller than *this. */
    inline Timestamp msec_ceil() const {
        uint32_t x = subsec() % subsec_per_msec;
        return (x ? *this + Timestamp(0, subsec_per_msec - x) : *this);
    }
    /** @brief Return the next microsecond-valued timestamp no smaller than *this. */
    inline Timestamp usec_ceil() const {
#if TIMESTAMP_NANOSEC
        uint32_t x = subsec() % subsec_per_usec;
        return (x ? *this + Timestamp(0, subsec_per_usec - x) : *this);
#else
        return *this;
#endif
    }
    /** @brief Return the next nanosecond-valued timestamp no smaller than *this. */
    inline Timestamp nsec_ceil() const {
        return *this;
    }

#if !CLICK_TOOL
    /** @brief Return a timestamp representing an interval of @a jiffies. */
    static inline Timestamp make_jiffies(click_jiffies_t jiffies);
    /** @overload */
    static inline Timestamp make_jiffies(click_jiffies_difference_t jiffies);
    /** @brief Return the number of jiffies represented by this timestamp. */
    inline click_jiffies_t jiffies() const;
#endif

    /** @brief Return a timestamp representing @a sec seconds. */
    static inline Timestamp make_sec(seconds_type sec) {
        return Timestamp(sec, 0);
    }
    /** @brief Return a timestamp representing @a sec seconds plus @a msec
     *  milliseconds.
     *  @pre 0 <= @a msec < 1000 */
    static inline Timestamp make_msec(seconds_type sec, uint32_t msec) {
        return Timestamp(sec, msec_to_subsec(msec));
    }
    /** @brief Return a timestamp representing @a msec milliseconds. */
    static inline Timestamp make_msec(value_type msec) {
        Timestamp t = Timestamp::uninitialized_t();
#if TIMESTAMP_REP_FLAT64
        t._t.x = msec * (subsec_per_sec / msec_per_sec);
#else
        value_div_mod(t._t.sec, t._t.subsec, msec, msec_per_sec);
        t._t.subsec *= subsec_per_sec / msec_per_sec;
#endif
        return t;
    }
    /** @brief Return a timestamp representing @a sec seconds plus @a usec
     *  microseconds.
     *  @pre 0 <= @a usec < 1000000 */
    static inline Timestamp make_usec(seconds_type sec, uint32_t usec) {
        return Timestamp(sec, usec_to_subsec(usec));
    }
    /** @brief Return a timestamp representing @a usec microseconds. */
    static inline Timestamp make_usec(value_type usec) {
        Timestamp t = Timestamp::uninitialized_t();
#if TIMESTAMP_REP_FLAT64
        t._t.x = usec * (subsec_per_sec / usec_per_sec);
#else
        value_div_mod(t._t.sec, t._t.subsec, usec, usec_per_sec);
        t._t.subsec *= subsec_per_sec / usec_per_sec;
#endif
        return t;
    }
    /** @brief Return a timestamp representing @a sec seconds plus @a nsec
     *  nanoseconds.
     *  @pre 0 <= @a nsec < 1000000000 */
    static inline Timestamp make_nsec(seconds_type sec, uint32_t nsec) {
        return Timestamp(sec, nsec_to_subsec(nsec));
    }
    /** @brief Return a timestamp representing @a nsec nanoseconds. */
    static inline Timestamp make_nsec(value_type nsec) {
        Timestamp t = Timestamp::uninitialized_t();
#if TIMESTAMP_REP_FLAT64
        t._t.x = value_div(nsec, nsec_per_sec / subsec_per_sec);
#else
        value_div_mod(t._t.sec, t._t.subsec, nsec, nsec_per_sec);
        t._t.subsec /= nsec_per_sec / subsec_per_sec;
#endif
        return t;
    }


    /** @brief Return the smallest nonzero timestamp, Timestamp(0, 1). */
    static inline Timestamp epsilon() {
        return Timestamp(0, 1);
    }

    /** @brief Clear this timestamp. */
    inline void clear() {
        assign(0, 0);
    }


    /** Set this timestamp to a seconds-and-subseconds value.
     *
     * @sa Timestamp(int, int) */
    inline void assign(seconds_type sec, uint32_t subsec = 0) {
#if TIMESTAMP_REP_FLAT64
        _t.x = (int64_t) sec * subsec_per_sec + subsec;
#else
        _t.sec = sec;
        _t.subsec = subsec;
#endif
    }
    /** Assign this timestamp to a seconds-and-microseconds value. */
    inline void assign_usec(seconds_type sec, uint32_t usec) {
        assign(sec, usec_to_subsec(usec));
    }
    /** Assign this timestamp to a seconds-and-nanoseconds value. */
    inline void assign_nsec(seconds_type sec, uint32_t nsec) {
        assign(sec, nsec_to_subsec(nsec));
    }

    /** @cond never */
    /** Assign this timestamp to a seconds-and-subseconds value.
     * @deprecated Use assign() instead. */
    inline void set(seconds_type sec, uint32_t subsec = 0) CLICK_DEPRECATED;
    /** Assign this timestamp to a seconds-and-microseconds value.
     * @deprecated Use assign_usec() instead. */
    inline void set_usec(seconds_type sec, uint32_t usec) CLICK_DEPRECATED;
    /** Assign this timestamp to a seconds-and-nanoseconds value.
     * @deprecated Use assign_nsec() instead. */
    inline void set_nsec(seconds_type sec, uint32_t nsec) CLICK_DEPRECATED;
    /** @brief Deprecated synonym for assign_now().
     * @deprecated Use Timestamp::assign_now() instead. */
    inline void set_now() CLICK_DEPRECATED;
    /** @endcond never */
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE && !CLICK_MINIOS
    int set_timeval_ioctl(int fd, int ioctl_selector);
#endif


    /** @brief Return the current system time.
     *
     * System time is measured in seconds since January 1, 1970 GMT.
     * Produces the most precise timestamp available.
     *
     * @note System time can jump forwards or backwards as a result of user
     * actions.  For a clock that never moves backwards, see now_steady().
     * @sa recent(), assign_now(), now_steady() */
    static inline Timestamp now();

    /** @brief Set this timestamp to the current system time.
     *
     * Like "*this = Timestamp::now()".
     * @sa now(), assign_recent() */
    inline void assign_now();

    /** @brief Return a recent system time.
     *
     * The Timestamp::now() function calculates the current system time, which
     * is relatively expensive.  Timestamp::recent() can be faster, but is
     * less precise: it returns a cached copy of a recent system time.
     * @sa now(), assign_recent() */
    static inline Timestamp recent();

    /** @brief Set this timestamp to a recent system time.
     *
     * Like "*this = Timestamp::recent()".
     * @sa recent(), assign_now() */
    inline void assign_recent();


    /** @brief Return the current steady-clock time.
     *
     * The steady clock, often called a monotonic clock, is a system clock
     * that never moves backwards.  Steady-clock time is measured in seconds
     * since an undefined start point (often related to the most recent boot).
     * Produces the most precise timestamp available.
     *
     * @note Steady-clock times and system times are incomparable, since they
     * have different start points.
     *
     * @sa recent_steady(), assign_now_steady() */
    static inline Timestamp now_steady();

    /** @brief Set this timestamp to the current steady-clock time.
     *
     * Like "*this = Timestamp::now_steady()".
     * @sa now_steady() */
    inline void assign_now_steady();

    /** @brief Return a recent steady-clock time.
     *
     * The Timestamp::now_steady() function calculates the current
     * steady-clock time, which is relatively expensive.
     * Timestamp::recent_steady() can be faster, but is less precise: it
     * returns a cached copy of a recent steady-clock time.
     * @sa now_steady(), assign_recent_steady() */
    static inline Timestamp recent_steady();

    /** @brief Set this timestamp to a recent steady-clock time.
     *
     * Like "*this = Timestamp::recent_steady()".
     * @sa recent_steady(), assign_now_steady() */
    inline void assign_recent_steady();


    /** @brief Unparse this timestamp into a String.
     *
     * Returns a string formatted like "10.000000", with at least six
     * subsecond digits.  (Nanosecond-precision timestamps where the number of
     * nanoseconds is not evenly divisible by 1000 are given nine subsecond
     * digits.) */
    String unparse() const;

    /** @brief Unparse this timestamp into a String as an interval.
     *
     * Returns a string formatted like "1us" or "1.000002s". */
    String unparse_interval() const;


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


    /** @brief  Type of a Timestamp representation.
     *
     * This type is rarely useful for Timestamp users; we export it to avoid
     * strict-aliasing warnings in unions. */
    union rep_t {
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
#if CLICK_LINUXMODULE
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
        ktime_t ktime;
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
        skb_timeval skbtime;
# endif
#endif
#if TIMESTAMP_PUNS_TIMEVAL
        struct timeval tv;
#elif TIMESTAMP_PUNS_TIMESPEC
        struct timespec tspec;
#endif
    };

#if TIMESTAMP_WARPABLE
    /** @name Timewarping */
    //@{
    enum warp_class_type {
        warp_none = 0,          ///< Run in real time (the default).
        warp_linear = 1,        ///< Run in speeded-up or slowed-down real time.
        warp_nowait = 2,        ///< Run in speeded-up or slowed-down real time,
                                //   but don't wait for timers.
        warp_simulation = 3     ///< Run in simulation time.
    };


    /** @brief Return the active timewarp class. */
    static inline int warp_class();

    /** @brief Return the timewarp speed.
     *
     * Timewarp speed measures how much faster Timestamp::now() appears to
     * move compared with wall-clock time.  Only meaningful if warp_class() is
     * #warp_linear or #warp_nowait. */
    static inline double warp_speed();


    /** @brief Set the timewarp class to @a w.
     * @param w warp class
     * @param s speed (\> 0, meaningful when @a w == #warp_linear)
     *
     * The timewarp classes are as follows:
     *
     * <dl>
     * <dt>#warp_none</dt>
     * <dd>Click time corresponds to real time.  This is the default.</dd>
     *
     * <dt>#warp_linear</dt>
     * <dd>Click time is a speeded-up or slowed-down version of real time.
     * The speedup factor is @a s.  If @a s \> 1, then time as measured by
     * Timestamp::now() will appear to move a factor of @a s faster than real
     * time: for instance, a Timer set using Timer::schedule_after_s(2) from
     * now will fire after just 1 second of wall-clock time.</dd>
     *
     * <dt>#warp_nowait</dt>
     * <dd>Like #warp_linear, but the Click driver never waits for a timer to
     * expire.  Instead, time appears to "jump" ahead to the next expiration
     * time.</dd>
     *
     * <dt>#warp_simulation</dt>
     * <dd>Click time is completely divorced from real time.  Every call to
     * Timestamp::now() appears to increase the current time by
     * Timestamp::epsilon() and the Click driver never waits for a timer to
     * expire.  This mode effectively turns Click into an event-driven
     * simulator.</dd>
     * </dl>
     */
    static void warp_set_class(warp_class_type w, double s = 1.0);

    /** @brief Reset current time.
     * @a t_system new system time
     * @a t_steady new steady-clock time
     *
     * Only usable when warp_class() is not #warp_none. */
    static void warp_set_now(const Timestamp &t_system, const Timestamp &t_steady);


    /** @brief Return the wall-clock time corresponding to a delay. */
    inline Timestamp warp_real_delay() const;

    /** @brief Return true iff time skips ahead around timer expirations. */
    static inline bool warp_jumping();

    /** @brief Move Click time past a timer expiration.
     *
     * Does nothing if warp_jumping() is false or @a expiry is in the past. */
    static void warp_jump_steady(const Timestamp &expiry);


    /** @brief Return the warp-free current system time.
     *
     * Like now(), but the time returned is unaffected by timewarping.
     * @sa now(), assign_now_unwarped() */
    static inline Timestamp now_unwarped();

    /** @brief Set this timestamp to the warp-free current system time.
     *
     * Like assign_now(), but the time assigned is unaffected by timewarping.
     * @sa assign_now(), now_unwarped() */
    inline void assign_now_unwarped();

    /** @brief Return the warp-free current steady-clock time.
     *
     * Like now_steady(), but the time returned is unaffected by timewarping.
     * @sa now_steady(), assign_now_steady_unwarped() */
    static inline Timestamp now_steady_unwarped();

    /** @brief Set this timestamp to the warp-free current steady-clock time.
     *
     * Like assign_now_steady(), but the time assigned is unaffected by
     * timewarping.
     * @sa assign_now_steady(), now_steady_unwarped() */
    inline void assign_now_steady_unwarped();
    //@}
#endif

  private:

    rep_t _t;

    inline void add_fix() {
#if TIMESTAMP_REP_FLAT64
        /* no fix necessary */
#elif TIMESTAMP_MATH_FLAT64
        if (_t.subsec >= subsec_per_sec)
            _t.x += (uint32_t) -subsec_per_sec;
#else
        if (_t.subsec >= subsec_per_sec)
            _t.sec++, _t.subsec -= subsec_per_sec;
#endif
    }

    inline void sub_fix() {
#if TIMESTAMP_REP_FLAT64
        /* no fix necessary */
#elif TIMESTAMP_MATH_FLAT64
        if (_t.subsec < 0)
            _t.subsec += subsec_per_sec;
#else
        if (_t.subsec < 0)
            _t.sec--, _t.subsec += subsec_per_sec;
#endif
    }

    static inline value_type value_div(value_type a, uint32_t b) {
        return int_divide(a, b);
    }

    static inline void value_div_mod(int32_t &div, int32_t &rem,
                                     value_type a, uint32_t b) {
        value_type quot;
        rem = int_remainder(a, b, quot);
        div = quot;
    }

    inline void assign_now(bool recent, bool steady, bool unwarped);

#if TIMESTAMP_WARPABLE
    static inline void warp_adjust(bool steady, const Timestamp &t_raw, const Timestamp &t_warped);
    inline Timestamp warped(bool steady) const;
    void warp(bool steady, bool from_now);
#endif

    friend inline bool operator==(const Timestamp &a, const Timestamp &b);
    friend inline bool operator<(const Timestamp &a, const Timestamp &b);
    friend inline Timestamp operator-(const Timestamp &b);
    friend inline Timestamp &operator+=(Timestamp &a, const Timestamp &b);
    friend inline Timestamp &operator-=(Timestamp &a, const Timestamp &b);

};


#if TIMESTAMP_WARPABLE
/** @cond never */
class TimestampWarp {
    static Timestamp::warp_class_type kind;
    static double speed;
    static Timestamp flat_offset[2];
    static double offset[2];
    friend class Timestamp;
};
/** @endcond never */

inline int Timestamp::warp_class() {
    return TimestampWarp::kind;
}

inline double Timestamp::warp_speed() {
    return TimestampWarp::speed;
}

inline bool Timestamp::warp_jumping() {
    return TimestampWarp::kind >= warp_nowait;
}

inline Timestamp Timestamp::warped(bool steady) const {
    Timestamp t = *this;
    if (TimestampWarp::kind)
        t.warp(steady, false);
    return t;
}
#endif


/** @brief Create a Timestamp measuring @a tv.
    @param tv timeval structure */
inline
Timestamp::Timestamp(const struct timeval& tv)
{
    assign(tv.tv_sec, usec_to_subsec(tv.tv_usec));
}

#if HAVE_STRUCT_TIMESPEC
/** @brief Create a Timestamp measuring @a ts.
    @param ts timespec structure */
inline
Timestamp::Timestamp(const struct timespec& ts)
{
    assign(ts.tv_sec, nsec_to_subsec(ts.tv_nsec));
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

inline void
Timestamp::assign_now(bool recent, bool steady, bool unwarped)
{
    (void) recent, (void) steady, (void) unwarped;

#if TIMESTAMP_PUNS_TIMESPEC
# define TIMESTAMP_DECLARE_TSP struct timespec &tsp = _t.tspec
# define TIMESTAMP_RESOLVE_TSP /* nothing */
#else
# define TIMESTAMP_DECLARE_TSP struct timespec ts, &tsp = ts
# define TIMESTAMP_RESOLVE_TSP assign(tsp.tv_sec, nsec_to_subsec(tsp.tv_nsec))
#endif
#if TIMESTAMP_PUNS_TIMEVAL
# define TIMESTAMP_DECLARE_TVP struct timeval &tvp = _t.tv
# define TIMESTAMP_RESOLVE_TVP /* nothing */
#else
# define TIMESTAMP_DECLARE_TVP struct timeval tv, &tvp = tv
# define TIMESTAMP_RESOLVE_TVP assign(tvp.tv_sec, usec_to_subsec(tvp.tv_usec))
#endif

#if CLICK_LINUXMODULE
# if !TIMESTAMP_NANOSEC
    if (!recent && !steady) {
        TIMESTAMP_DECLARE_TVP;
        do_gettimeofday(&tvp);
        TIMESTAMP_RESOLVE_TVP;
        return;
    }
# endif
    TIMESTAMP_DECLARE_TSP;
    if (recent && steady) {
# if HAVE_LINUX_GET_MONOTONIC_COARSE
        tsp = get_monotonic_coarse();
# elif HAVE_LINUX_GETBOOTTIME && HAVE_LINUX_KTIME_MONO_TO_ANY
        // XXX Is this even worth it????? Would it be faster to just get the
        // current time?
        tsp = current_kernel_time();
        struct timespec delta =
            ktime_to_timespec(ktime_mono_to_any(ktime_set(0, 0), TK_OFFS_REAL));
        set_normalized_timespec(&tsp, tsp.tv_sec - delta.tv_sec,
                                tsp.tv_nsec - delta.tv_nsec);
# elif HAVE_LINUX_GETBOOTTIME
        tsp = current_kernel_time();
        struct timespec delta;
        getboottime(&delta);
        monotonic_to_bootbased(&delta);
        set_normalized_timespec(&tsp, tsp.tv_sec - delta.tv_sec,
                                tsp.tv_nsec - delta.tv_nsec);
# else
        // older kernels don't export enough information to produce a recent
        // steady timestamp; produce a current steady timestamp
        ktime_get_ts(&tsp);
# endif
    } else if (recent)
        tsp = current_kernel_time();
    else if (steady)
        ktime_get_ts(&tsp);
    else
        getnstimeofday(&tsp);
    TIMESTAMP_RESOLVE_TSP;

#elif TIMESTAMP_NANOSEC && CLICK_BSDMODULE
    TIMESTAMP_DECLARE_TSP;
    if (recent && steady)
        getnanouptime(&tsp);
    else if (recent)
        getnanotime(&tsp);
    else if (steady)
        nanouptime(&tsp);
    else
        nanotime(&tsp);
    TIMESTAMP_RESOLVE_TSP;

#elif CLICK_BSDMODULE
    TIMESTAMP_DECLARE_TVP;
    if (recent && steady)
        getmicrouptime(&tvp);
    else if (recent)
        getmicrotime(&tvp);
    else if (steady)
        microuptime(&tvp);
    else
        microtime(&tvp);
    TIMESTAMP_RESOLVE_TVP;

#elif CLICK_MINIOS
    TIMESTAMP_DECLARE_TVP;
    gettimeofday(&tvp, (struct timezone *) 0);
    TIMESTAMP_RESOLVE_TVP;

#elif CLICK_NS
    if (schedule_granularity == usec_per_sec) {
        TIMESTAMP_DECLARE_TVP;
        simclick_gettimeofday(&tvp);
        TIMESTAMP_RESOLVE_TVP;
    } else {
        assert(0 && "nanosecond precision not available yet");
    }

#elif HAVE_USE_CLOCK_GETTIME
    TIMESTAMP_DECLARE_TSP;
    if (steady)
        clock_gettime(CLOCK_MONOTONIC, &tsp);
    else
        clock_gettime(CLOCK_REALTIME, &tsp);
    TIMESTAMP_RESOLVE_TSP;

#else
    TIMESTAMP_DECLARE_TVP;
    gettimeofday(&tvp, (struct timezone *) 0);
    TIMESTAMP_RESOLVE_TVP;
#endif

#undef TIMESTAMP_DECLARE_TSP
#undef TIMESTAMP_RESOLVE_TSP
#undef TIMESTAMP_DECLARE_TVP
#undef TIMESTAMP_RESOLVE_TVP

#if TIMESTAMP_WARPABLE
    // timewarping
    if (!unwarped && TimestampWarp::kind)
        warp(steady, true);
#endif
}

inline void
Timestamp::assign_now()
{
    assign_now(false, false, false);
}

inline Timestamp
Timestamp::now()
{
    Timestamp t = Timestamp::uninitialized_t();
    t.assign_now();
    return t;
}

inline void
Timestamp::assign_recent()
{
    assign_now(true, false, false);
}

inline Timestamp
Timestamp::recent()
{
    Timestamp t = Timestamp::uninitialized_t();
    t.assign_recent();
    return t;
}

inline void
Timestamp::assign_now_steady()
{
    assign_now(false, true, false);
}

inline Timestamp
Timestamp::now_steady()
{
    Timestamp t = Timestamp::uninitialized_t();
    t.assign_now_steady();
    return t;
}

inline void
Timestamp::assign_recent_steady()
{
    assign_now(true, true, false);
}

inline Timestamp
Timestamp::recent_steady()
{
    Timestamp t = Timestamp::uninitialized_t();
    t.assign_recent_steady();
    return t;
}

#if TIMESTAMP_WARPABLE
inline void
Timestamp::assign_now_unwarped()
{
    assign_now(false, false, true);
}

inline Timestamp
Timestamp::now_unwarped()
{
    Timestamp t = Timestamp::uninitialized_t();
    t.assign_now_unwarped();
    return t;
}

inline void
Timestamp::assign_now_steady_unwarped()
{
    assign_now(false, true, true);
}

inline Timestamp
Timestamp::now_steady_unwarped()
{
    Timestamp t = Timestamp::uninitialized_t();
    t.assign_now_steady_unwarped();
    return t;
}
#endif

/** @brief Set this timestamp's seconds component.

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
    seconds_type s = sec();
    _t.x = (int64_t) s * subsec_per_sec + subsec;
#else
    _t.subsec = subsec;
#endif
}



/** @brief Return this timestamp's subseconds component. */
inline uint32_t
Timestamp::subsec() const
{
#if TIMESTAMP_REP_FLAT64
    return _t.x - (uint32_t) sec() * subsec_per_sec;
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
    return value_div(_t.x, subsec_per_sec / msec_per_sec);
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
    return value_div(_t.x, subsec_per_sec / usec_per_sec);
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

#if !CLICK_TOOL
inline click_jiffies_t
Timestamp::jiffies() const
{
# if TIMESTAMP_REP_FLAT64
    // This is not very precise when CLICK_HZ doesn't divide NSUBSEC evenly.
    return value_div(_t.x, subsec_per_sec / CLICK_HZ);
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
    // Not very precise when CLICK_HZ doesn't evenly divide subsec_per_sec.
    Timestamp t = Timestamp::uninitialized_t();
# if TIMESTAMP_REP_FLAT64
    t._t.x = (int64_t) jiffies * (subsec_per_sec / CLICK_HZ);
# else
    t._t.sec = jiffies / CLICK_HZ;
    t._t.subsec = (jiffies - t._t.sec * CLICK_HZ) * (subsec_per_sec / CLICK_HZ);
# endif
    return t;
}

inline Timestamp
Timestamp::make_jiffies(click_jiffies_difference_t jiffies)
{
    // Not very precise when CLICK_HZ doesn't evenly divide subsec_per_sec.
    Timestamp t = Timestamp::uninitialized_t();
# if TIMESTAMP_REP_FLAT64
    t._t.x = (int64_t) jiffies * (subsec_per_sec / CLICK_HZ);
# else
    if (jiffies < 0)
        t._t.sec = -(-(jiffies + 1) / CLICK_HZ) - 1;
    else
        t._t.sec = jiffies / CLICK_HZ;
    t._t.subsec = (jiffies - t._t.sec * CLICK_HZ) * (subsec_per_sec / CLICK_HZ);
# endif
    return t;
}
#endif

/** @cond never */
inline void Timestamp::set_now() {
    assign_now(false, false, false);
}

inline void Timestamp::set(seconds_type sec, uint32_t subsec) {
    assign(sec, subsec);
}

inline void Timestamp::set_usec(seconds_type sec, uint32_t usec) {
    assign_usec(sec, usec);
}

inline void Timestamp::set_nsec(seconds_type sec, uint32_t nsec) {
    assign_nsec(sec, nsec);
}
/** @endcond never */

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

/** @overload */
inline bool
operator<(const Timestamp &a, int b)
{
    return a < Timestamp(b);
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

/** @overload */
inline bool
operator<=(const Timestamp &a, int b)
{
    return a <= Timestamp(b);
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

/** @overload */
inline bool
operator>=(const Timestamp &a, int b)
{
    return a >= Timestamp(b);
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

/** @overload */
inline bool
operator>(const Timestamp &a, int b)
{
    return a > Timestamp(b);
}

/** @brief Add @a b to @a a.

    Returns the result (the new value of @a a). */
inline Timestamp &
operator+=(Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_REP_FLAT64 || TIMESTAMP_MATH_FLAT64
    a._t.x += b._t.x;
#else
    a._t.sec += b._t.sec;
    a._t.subsec += b._t.subsec;
#endif
    a.add_fix();
    return a;
}

/** @brief Subtract @a b from @a a.

    Returns the result (the new value of @a a). */
inline Timestamp &
operator-=(Timestamp &a, const Timestamp &b)
{
#if TIMESTAMP_REP_FLAT64 || TIMESTAMP_MATH_FLAT64
    a._t.x -= b._t.x;
#else
    a._t.sec -= b._t.sec;
    a._t.subsec -= b._t.subsec;
#endif
    a.sub_fix();
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
        return Timestamp(-(a.sec() + 1), Timestamp::subsec_per_sec - a.subsec());
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
    _t.x = (int64_t) floor(d * subsec_per_sec + 0.5);
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

#if TIMESTAMP_WARPABLE
inline Timestamp
Timestamp::warp_real_delay() const
{
    if (likely(!TimestampWarp::kind) || TimestampWarp::speed == 1.0)
        return *this;
    else
        return *this / TimestampWarp::speed;
}
#endif

#if TIMESTAMP_PUNS_TIMEVAL
inline const struct timeval &
Timestamp::timeval() const
{
    return _t.tv;
}

inline const struct timeval &
Timestamp::timeval_ceil() const
{
    return _t.tv;
}
#else
/** @brief Return a struct timeval that approximates this timestamp.

    If Timestamp and struct timeval have the same size and representation,
    then this operation returns a "const struct timeval &" whose address is
    the same as this Timestamp. If Timestamps have nanosecond precision,
    the conversion rounds down, so Timestamp(t.timeval()) <= t. */
inline struct timeval
Timestamp::timeval() const
{
    struct timeval tv;
    tv.tv_sec = sec();
    tv.tv_usec = usec();
    return tv;
}

/** @brief Return the minimum struct timeval >= this timestamp.

    If Timestamp and struct timeval have the same size and representation,
    then this operation returns a "const struct timeval &" whose address is
    the same as this Timestamp. */
inline struct timeval
Timestamp::timeval_ceil() const
{
    return (*this + Timestamp(0, subsec_per_usec - 1)).timeval();
}
#endif

#if HAVE_STRUCT_TIMESPEC
# if TIMESTAMP_PUNS_TIMESPEC
inline const struct timespec &
Timestamp::timespec() const
{
    return _t.tspec;
}
# else
/** @brief Return a struct timespec with the same value as this timestamp.

    If Timestamp and struct timespec have the same size and representation,
    then this operation returns a "const struct timespec &" whose address is
    the same as this Timestamp. */
inline struct timespec
Timestamp::timespec() const
{
    struct timespec ts;
    ts.tv_sec = sec();
    ts.tv_nsec = nsec();
    return ts;
}
# endif
#endif


class ArgContext;
extern const ArgContext blank_args;
bool cp_time(const String &str, Timestamp *result, bool allow_negative);

/** @class TimestampArg
  @brief Parser class for timestamps. */
class TimestampArg { public:
    TimestampArg(bool is_signed = false)
        : is_signed(is_signed) {
    }
    bool parse(const String &str, Timestamp &value, const ArgContext &args = blank_args) {
        (void) args;
        return cp_time(str, &value, is_signed);
    }
    bool is_signed;
};

template<> struct DefaultArg<Timestamp> : public TimestampArg {};
template<> struct has_trivial_copy<Timestamp> : public true_type {};

CLICK_ENDDECLS
#endif
