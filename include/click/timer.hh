// -*- c-basic-offset: 4; related-file-name: "../../lib/timer.cc" -*-
#ifndef CLICK_TIMER_HH
#define CLICK_TIMER_HH
#include <click/sync.hh>
#include <click/glue.hh>
#include <click/element.hh>
CLICK_DECLS
class Element;
class Router;
class Timer;
class Task;

typedef void (*TimerHook)(Timer *, void *);

class Timer { public:

    Timer(TimerHook, void *);
    Timer(Element *);			// call element->run_timer()
    Timer(Task *);			// call task->reschedule()
    ~Timer()				{ if (scheduled()) unschedule(); }

    bool initialized() const		{ return _router != 0; }
    bool scheduled() const		{ return _schedpos >= 0; }
    const timeval &expiry() const	{ return _expiry; }
  
    inline void initialize(Router *);
    inline void initialize(Element *);
    void cleanup()			{ unschedule(); }
    void uninitialize()			{ cleanup(); }	// deprecated

    void schedule_at(const timeval &);
    inline void reschedule_at(const timeval &); // synonym

    inline void schedule_now();
    void schedule_after(const timeval &);
    inline void schedule_after_s(uint32_t);
    inline void schedule_after_ms(uint32_t);
    inline void reschedule_after(const timeval &);
    void reschedule_after_s(uint32_t);
    void reschedule_after_ms(uint32_t);

    void unschedule();
  
  private:
  
    int _schedpos;
    timeval _expiry;
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
Timer::schedule_now()
{
    schedule_after_ms(0);
}

inline void
Timer::schedule_after_s(uint32_t s)
{
    schedule_after(make_timeval(s, 0));
}

inline void
Timer::schedule_after_ms(uint32_t ms)
{
    schedule_after(make_timeval(ms / 1000, (ms % 1000) * 1000));
}

inline void
Timer::reschedule_after(const timeval &delta)
{
    schedule_at(_expiry + delta);
}

inline void
Timer::reschedule_at(const timeval &tv)
{
    schedule_at(tv);
}

CLICK_ENDDECLS
#endif
