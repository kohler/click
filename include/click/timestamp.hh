// -*- c-basic-offset: 4; related-file-name: "../../lib/timestamp.cc" -*-
#ifndef CLICK_TIMESTAMP_HH
#define CLICK_TIMESTAMP_HH
#include <click/glue.hh>
CLICK_DECLS
class String;
#if !HAVE_NANOTIMESTAMP && SIZEOF_STRUCT_TIMEVAL == 8
# define TIMESTAMP_PUNS_TIMEVAL 1
#endif

#if HAVE_NANOTIMESTAMP
# define PRITIMESTAMP "%d.%09d"
#else
# define PRITIMESTAMP "%d.%06d"
#endif

class Timestamp { public:

#if HAVE_NANOTIMESTAMP
    enum { SUBSEC_PER_SEC = 1000000000 };
    inline static int32_t msec_to_subsec(int32_t m)	{ return m * 1000000; }
    inline static int32_t usec_to_subsec(int32_t u)	{ return u * 1000; }
    inline static int32_t nsec_to_subsec(int32_t n)	{ return n; }
    inline static int32_t subsec_to_msec(int32_t ss)	{ return ss / 1000000;}
    inline static int32_t subsec_to_usec(int32_t ss)	{ return ss / 1000; }
    inline static int32_t subsec_to_nsec(int32_t ss)	{ return ss; }
#else
    enum { SUBSEC_PER_SEC = 1000000 };
    inline static int32_t msec_to_subsec(int32_t m)	{ return m * 1000; }
    inline static int32_t usec_to_subsec(int32_t u)	{ return u; }
    inline static int32_t nsec_to_subsec(int32_t n)	{ return (n + 500) / 1000; }
    inline static int32_t subsec_to_msec(int32_t ss)	{ return ss / 1000; }
    inline static int32_t subsec_to_usec(int32_t ss)	{ return ss; }
    inline static int32_t subsec_to_nsec(int32_t ss)	{ return ss * 1000; }
#endif
    
    Timestamp()				: _sec(0), _subsec(0) { }
    Timestamp(int32_t s, int32_t ss)	: _sec(s), _subsec(ss) { }
    Timestamp(const struct timeval& tv)	: _sec(tv.tv_sec), _subsec(usec_to_subsec(tv.tv_usec)) { }
#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
    inline Timestamp(double);
#endif

    operator bool() const		{ return _sec || _subsec; }
#if TIMESTAMP_PUNS_TIMEVAL
    const timeval& to_timeval() const	{ return *(const timeval*) this; }
#else
    inline timeval to_timeval() const;
#endif
    inline double to_double() const;
    
    static inline Timestamp now();
    static Timestamp epsilon()		{ return Timestamp(0, 1); }

    static inline Timestamp make_msec(uint32_t ms);
    static inline Timestamp make_usec(int32_t s, int32_t us);
    static inline Timestamp make_usec(uint32_t us);
    static inline Timestamp make_nsec(int32_t s, int32_t ns);
    
    inline void set_now();
    void set(int32_t s, int32_t ss)		{ _sec = s; _subsec = ss; }
    void set_sec(int32_t s)			{ _sec = s; }
    void set_subsec(int32_t ss)			{ _subsec = ss; }
    void set_usec(int32_t s, int32_t us)	{ _sec = s; _subsec = usec_to_subsec(us); }
    void set_nsec(int32_t s, int32_t ns)	{ _sec = s; _subsec = nsec_to_subsec(ns); }

    int32_t sec() const		{ return _sec; }
    int32_t subsec() const	{ return _subsec; }
    
    int32_t msec() const	{ return subsec_to_msec(_subsec); }
    int32_t msec1() const	{ return _sec*1000 + subsec_to_msec(_subsec); }
    int32_t usec() const	{ return subsec_to_usec(_subsec); }
    int32_t usec1() const	{ return _sec*1000000 + subsec_to_usec(_subsec); }
    int32_t nsec() const	{ return subsec_to_nsec(_subsec); }
    int32_t nsec1() const	{ return _sec*1000000000 + subsec_to_nsec(_subsec); }

    void add_fix();
    void sub_fix();
#if HAVE_NANOTIMESTAMP
    void convert_to_timeval()	{ _subsec /= 1000; }
#else
    void convert_to_timeval()	{ }
#endif

    String unparse() const;

    int32_t _sec;
    int32_t _subsec;
    
};

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

inline Timestamp
Timestamp::now()
{
    Timestamp t;
    t.set_now();
    return t;
}

inline Timestamp
Timestamp::make_msec(uint32_t ms)
{
    return Timestamp(ms/1000, msec_to_subsec(ms % 1000));
}

inline Timestamp
Timestamp::make_usec(int32_t s, int32_t us)
{
    return Timestamp(s, usec_to_subsec(us));
}

inline Timestamp
Timestamp::make_usec(uint32_t us)
{
    return Timestamp(us/1000000, usec_to_subsec(us % 1000000));
}

inline Timestamp
Timestamp::make_nsec(int32_t s, int32_t ns)
{
    return Timestamp(s, nsec_to_subsec(ns));
}

#if HAVE_NANOTIMESTAMP
inline struct timeval
Timestamp::to_timeval() const
{
    struct timeval tv;
    tv.tv_sec = _sec;
    tv.tv_usec = usec();
    return tv;
}
#endif

inline bool
operator==(const Timestamp &a, const Timestamp &b)
{
    return a.sec() == b.sec() && a.subsec() == b.subsec();
}

inline bool
operator!=(const Timestamp &a, const Timestamp &b)
{
    return a.sec() != b.sec() || a.subsec() != b.subsec();
}

inline bool
operator<(const Timestamp &a, const Timestamp &b)
{
    return a.sec() < b.sec() || (a.sec() == b.sec() && a.subsec() < b.subsec());
}

inline bool
operator<=(const Timestamp &a, const Timestamp &b)
{
    return a.sec() < b.sec() || (a.sec() == b.sec() && a.subsec() <= b.subsec());
}

inline bool
operator>=(const Timestamp &a, const Timestamp &b)
{
    return a.sec() > b.sec() || (a.sec() == b.sec() && a.subsec() >= b.subsec());
}

inline bool
operator>(const Timestamp &a, const Timestamp &b)
{
    return a.sec() > b.sec() || (a.sec() == b.sec() && a.subsec() > b.subsec());
}

inline void
Timestamp::add_fix()
{
    if (_subsec >= SUBSEC_PER_SEC)
	_sec++, _subsec -= SUBSEC_PER_SEC;
}

inline void
Timestamp::sub_fix()
{
    if (_subsec < 0)
	_sec--, _subsec += SUBSEC_PER_SEC;
}

inline Timestamp &
operator+=(Timestamp &a, const Timestamp &b)
{
    a._sec += b._sec;
    a._subsec += b._subsec;
    a.add_fix();
    return a;
}

inline Timestamp &
operator-=(Timestamp &a, const Timestamp &b)
{
    a._sec -= b._sec;
    a._subsec -= b._subsec;
    a.sub_fix();
    return a;
}

inline Timestamp
operator+(Timestamp a, const Timestamp &b)
{
    a += b;
    return a;
}

inline Timestamp
operator-(Timestamp a, const Timestamp &b)
{
    a -= b;
    return a;
}

#if !CLICK_LINUXMODULE && !CLICK_BSDMODULE
inline double
Timestamp::to_double() const
{
    return _sec + (_subsec / (double) SUBSEC_PER_SEC);
}

inline
Timestamp::Timestamp(double d)
    : _sec((uint32_t) d), _subsec((uint32_t) ((d - _sec) * SUBSEC_PER_SEC + 0.5))
{
}

inline Timestamp
operator*(const Timestamp &a, double b)
{
    return Timestamp(a.to_double() * b);
}

inline Timestamp
operator*(double a, const Timestamp &b)
{
    return Timestamp(b.to_double() * a);
}

inline Timestamp
operator*(int a, const Timestamp &b)
{
    return Timestamp(b.to_double() * a);
}

inline Timestamp
operator/(const Timestamp &a, double b)
{
    return Timestamp(a.to_double() / b);
}

inline Timestamp
operator/(const Timestamp &a, int b)
{
    return Timestamp(a.to_double() / b);
}

inline Timestamp
operator/(const Timestamp &a, unsigned b)
{
    return Timestamp(a.to_double() / b);
}

inline double
operator/(const Timestamp &a, const Timestamp &b)
{
    return a.to_double() / b.to_double();
}
# endif /* !CLICK_LINUXMODULE && !CLICK_BSDMODULE */

StringAccum& operator<<(StringAccum&, const Timestamp&);

CLICK_ENDDECLS
#endif
