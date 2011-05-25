// -*- related-file-name: "../../lib/timerset.cc" -*-
#ifndef CLICK_TIMERSET_HH
#define CLICK_TIMERSET_HH 1
#include <click/timer.hh>
#include <click/sync.hh>
#include <click/vector.hh>
CLICK_DECLS
class Router;
class RouterThread;
class Timer;

class TimerSet { public:

    TimerSet();

    Timestamp next_timer_expiry() const		{ return _timer_expiry; }
    inline Timestamp next_timer_expiry_adjusted() const;
#if CLICK_USERLEVEL
    inline int next_timer_delay(bool more_tasks, Timestamp &t) const;
#endif

    Timer *next_timer();			// useful for benchmarking
    const Timestamp &timer_check() const	{ return _timer_check; }

    unsigned max_timer_stride() const		{ return _max_timer_stride; }
    unsigned timer_stride() const		{ return _timer_stride; }
    void set_max_timer_stride(unsigned timer_stride);

    void kill_router(Router *router);

    void run_timers(RouterThread *thread, Master *master);

    inline void lock_timers();
    inline bool attempt_lock_timers();
    inline void unlock_timers();

  private:

    struct heap_element {
	Timestamp expiry;
	Timer *t;
#if SIZEOF_VOID_P == 4
	uint32_t padding; /* the structure should have size 16 */
#endif
	heap_element(Timer *t_)
	    : expiry(t_->expiry()), t(t_) {
	}
    };
    struct heap_less {
	inline bool operator()(const heap_element &a, const heap_element &b) {
	    return a.expiry < b.expiry;
	}
    };
    struct heap_place {
	inline void operator()(heap_element *begin, heap_element *t) {
	    t->t->_schedpos1 = (t - begin) + 1;
	}
    };

    // Most likely _timer_expiry now fits in a cache line
    Timestamp _timer_expiry CLICK_ALIGNED(8);

    unsigned _max_timer_stride;
    unsigned _timer_stride;
    unsigned _timer_count;
    Vector<heap_element> _timer_heap;
    Vector<Timer *> _timer_runchunk;
#if CLICK_LINUXMODULE
    spinlock_t _timer_lock;
    struct task_struct *_timer_task;
#elif HAVE_MULTITHREAD
    Spinlock _timer_lock;
#endif
    Timestamp _timer_check;
    uint32_t _timer_check_reports;

    inline void run_one_timer(Timer *);

    void set_timer_expiry() {
	if (_timer_heap.size())
	    _timer_expiry = _timer_heap.at_u(0).expiry;
	else
	    _timer_expiry = Timestamp();
    }
    void check_timer_expiry(Timer *t);

    friend class Timer;

};

inline Timestamp
TimerSet::next_timer_expiry_adjusted() const
{
    Timestamp e = _timer_expiry;
#if CLICK_USERLEVEL
    if (likely(!Timestamp::warp_jumping())) {
#endif
    if (_timer_stride >= 8 || e.sec() == 0)
	/* do nothing */;
    else if (_timer_stride >= 4)
	e -= Timer::adjustment();
    else
	e -= Timer::adjustment() + Timer::adjustment();
#if CLICK_USERLEVEL
    }
#endif
    return e;
}

inline void
TimerSet::lock_timers()
{
#if CLICK_LINUXMODULE
    if (current != _timer_task)
	spin_lock(&_timer_lock);
#elif HAVE_MULTITHREAD
    _timer_lock.acquire();
#endif
}

inline bool
TimerSet::attempt_lock_timers()
{
#if CLICK_LINUXMODULE
    return spin_trylock(&_timer_lock);
#elif HAVE_MULTITHREAD
    return _timer_lock.attempt();
#else
    return true;
#endif
}

inline void
TimerSet::unlock_timers()
{
#if CLICK_LINUXMODULE
    if (current != _timer_task)
	spin_unlock(&_timer_lock);
#elif HAVE_MULTITHREAD
    _timer_lock.release();
#endif
}

inline Timer *
TimerSet::next_timer()
{
    lock_timers();
    Timer *t = _timer_heap.empty() ? 0 : _timer_heap.at_u(0).t;
    unlock_timers();
    return t;
}

CLICK_ENDDECLS
#endif
