// -*- c-basic-offset: 4; related-file-name: "../../lib/timer.cc" -*-
#ifndef CLICK_TIMER_HH
#define CLICK_TIMER_HH
#include <click/sync.hh>
#include <click/glue.hh>
#include <click/element.hh>
#include <click/timestamp.hh>
CLICK_DECLS
class Element;
class Router;
class Timer;
class Task;

typedef void (*TimerHook)(Timer *, void *);

class Timer { public:

    Timer(TimerHook, void*);
    Timer(Element*);			// call element->run_timer()
    Timer(Task*);			// call task->reschedule()
    ~Timer()				{ if (scheduled()) unschedule(); }

    bool initialized() const		{ return _router != 0; }
    bool scheduled() const		{ return _schedpos >= 0; }
    const Timestamp &expiry() const	{ return _expiry; }
  
    inline void initialize(Router*);
    inline void initialize(Element*);
    void cleanup()			{ unschedule(); }
    void uninitialize()			{ cleanup(); }	// deprecated

    void schedule_at(const Timestamp&);
    inline void reschedule_at(const Timestamp&); // synonym

    inline void schedule_now();
    void schedule_after(const Timestamp&);
    inline void schedule_after_s(uint32_t);
    inline void schedule_after_ms(uint32_t);
    inline void reschedule_after(const Timestamp&);
    inline void reschedule_after_s(uint32_t);
    inline void reschedule_after_ms(uint32_t);

    void unschedule();
  
  private:
  
    int _schedpos;
    Timestamp _expiry;
    TimerHook _hook;
    void *_thunk;
    Router *_router;

    Timer(const Timer &);
    Timer &operator=(const Timer &);

    friend class Master;
  
};

inline void
Timer::initialize(Router *router)
{
    assert(!initialized());
    _router = router;
}

inline void
Timer::initialize(Element *element)
{
    initialize(element->router());
}

inline void
Timer::reschedule_at(const Timestamp &ts)
{
    schedule_at(ts);
}

inline void
Timer::schedule_now()
{
    schedule_after_ms(0);
}

inline void
Timer::schedule_after_s(uint32_t s)
{
    schedule_after(Timestamp(s, 0));
}

inline void
Timer::schedule_after_ms(uint32_t ms)
{
    schedule_after(Timestamp::make_msec(ms));
}

inline void
Timer::reschedule_after(const Timestamp &delta)
{
    schedule_at(_expiry + delta);
}

inline void
Timer::reschedule_after_s(uint32_t s)
{
    schedule_at(Timestamp(_expiry.sec() + s, _expiry.subsec()));
}

inline void
Timer::reschedule_after_ms(uint32_t ms)
{
    schedule_at(_expiry + Timestamp::make_msec(ms));
}

CLICK_ENDDECLS
#endif
