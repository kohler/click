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
    inline ~Timer();

    inline bool initialized() const;
    inline bool scheduled() const;
    inline const Timestamp &expiry() const;
  
    inline void initialize(Router*);
    inline void initialize(Element*);

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

/** @brief Destroy a Timer, unscheduling it first if necessary. */
inline
Timer::~Timer()
{
    if (scheduled())
	unschedule();
}

/** @brief Returns true iff the Timer has been initialized. */
inline bool
Timer::initialized() const
{
    return _router != 0;
}

/** @brief Returns true iff the Timer is currently scheduled. */
inline bool
Timer::scheduled() const
{
    return _schedpos >= 0;
}

/** @brief Returns the Timer's current expiration time.
 *
 * The expiration time is the absolute time at which the timer is next
 * scheduled to fire.  If the timer is not currently scheduled, then expiry()
 * returns the last assigned expiration time. */
inline const Timestamp &
Timer::expiry() const
{
    return _expiry;
}

/** @brief Initialize the timer.
 *
 * @param router the containing router
 *
 * Before a timer can be used, it must be attached to a containing router.
 * When that router is destroyed, the timer will automatically be destroyed as
 * well.
 */
inline void
Timer::initialize(Router *router)
{
    assert(!initialized());
    _router = router;
}

/** @brief Initialize the timer.
 *
 * @param element identifies the containing router
 *
 * @sa initialize(Router *router)
 */
inline void
Timer::initialize(Element *element)
{
    initialize(element->router());
}

/** @brief Schedule the timer to fire at @a when.
 *
 * @param when expiration time
 *
 * This element is a synonym for schedule_at(). */
inline void
Timer::reschedule_at(const Timestamp &when)
{
    schedule_at(when);
}

/** @brief Shedule the timer to fire immediately.
 *
 * Equivalent to schedule_at(Timestamp::now()). */
inline void
Timer::schedule_now()
{
    schedule_after(Timestamp::now());
}

/** @brief Schedule the timer to fire after @a delta_sec seconds.
 *
 * @param delta_sec interval until expiration time, in seconds
 *
 * @sa schedule_after, reschedule_after_s */
inline void
Timer::schedule_after_s(uint32_t delta_sec)
{
    schedule_after(Timestamp(delta_sec, 0));
}

/** @brief Schedule the timer to fire after @a delta_ms milliseconds.
 *
 * @param delta_ms interval until expiration time, in milliseconds
 *
 * @sa schedule_after */
inline void
Timer::schedule_after_ms(uint32_t delta_ms)
{
    schedule_after(Timestamp::make_msec(delta_ms));
}

/** @brief Schedule the timer to fire @a delta time after its previous expiry
 * time.
 *
 * @param delta interval until expiration time
 *
 * @sa schedule_after */
inline void
Timer::reschedule_after(const Timestamp &delta)
{
    schedule_at(_expiry + delta);
}

/** @brief Schedule the timer to fire @a delta_sec seconds after its previous
 * expiry time.
 *
 * @param delta_sec interval until expiration time, in seconds
 *
 * @sa schedule_after_s */
inline void
Timer::reschedule_after_s(uint32_t delta_sec)
{
    schedule_at(Timestamp(_expiry.sec() + delta_sec, _expiry.subsec()));
}

/** @brief Schedule the timer to fire @a delta_ms milliseconds after its
 * previous expiry time.
 *
 * @param delta_ms interval until expiration time, in milliseconds
 *
 * @sa schedule_after_ms */
inline void
Timer::reschedule_after_ms(uint32_t delta_ms)
{
    schedule_at(_expiry + Timestamp::make_msec(delta_ms));
}

CLICK_ENDDECLS
#endif
