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

typedef void (*TimerCallback)(Timer *, void *);
typedef TimerCallback TimerHook CLICK_DEPRECATED;

class Timer { public:

    /** @brief Construct a Timer that does nothing when fired. */
    Timer();

    /** @brief Construct a Timer that calls @a f(this, @a user_data) when
     * fired.
     * @param f callback function
     * @param user_data argument for callback function */
    Timer(TimerCallback f, void *user_data);

    /** @brief Construct a Timer that calls @a element ->@link
     * Element::run_timer() run_timer@endlink(this) when fired.
     * @param element the element */
    Timer(Element *element);

    /** @brief Construct a Timer that schedules @a task when fired.
     * @param task the task */
    Timer(Task *task);

    /** @brief Destroy a Timer, unscheduling it first if necessary. */
    inline ~Timer() {
	if (scheduled())
	    unschedule();
    }


    /** @brief Change the Timer to call @a f(this, @a user_data) when fired.
     * @param f callback function
     * @param user_data argument for callback function */
    inline void assign(TimerCallback f, void *user_data) {
	_hook = f;
	_thunk = user_data;
    }

    /** @brief Change the Timer to call @a element ->@link
     * Element::run_timer() run_timer@endlink(this) when fired.
     * @param element the element */
    void assign(Element *element);

    /** @brief Change the Timer to schedule @a task when fired.
     * @param task the task */
    void assign(Task *task);

    
    /** @brief Return true iff the Timer has been initialized. */
    inline bool initialized() const {
	return _router != 0;
    }

    /** @brief Return true iff the Timer is currently scheduled. */
    inline bool scheduled() const {
	return _schedpos >= 0;
    }

    /** @brief Return the Timer's current expiration time.
     *
     * The expiration time is the absolute time at which the timer is next
     * scheduled to fire.  If the timer is not currently scheduled, then
     * expiry() returns the last assigned expiration time. */
    inline const Timestamp &expiry() const {
	return _expiry;
    }


    /** @brief Initialize the timer.
     * @param router the containing router
     *
     * Before a timer can be used, it must be attached to a containing router.
     * When that router is destroyed, the timer is automatically
     * unscheduled.  It is safe to initialize the timer multiple times
     * on the same router. */
    inline void initialize(Router *router) {
	assert(!initialized() || _router == router);
	_router = router;
    }

    /** @brief Initialize the timer.
     * @param element identifies the containing router
     *
     * @sa initialize(Router *router) */
    inline void initialize(Element *element) {
	initialize(element->router());
    }


    /** @brief Schedule the timer to fire at @a when.
     * @param when expiration time
     *
     * If @a when is more than 2 seconds behind system time, then the
     * expiration time is silently updated to the current system time. */
    void schedule_at(const Timestamp &when);

    /** @brief Schedule the timer to fire at @a when.
     * @param when expiration time
     *
     * This element is a synonym for schedule_at(). */
    inline void reschedule_at(const Timestamp &when) {
	schedule_at(when);
    }

    /** @brief Shedule the timer to fire immediately.
     *
     * Equivalent to schedule_at(Timestamp::now()). */
    inline void schedule_now() {
	schedule_at(Timestamp::now());
    }

    /** @brief Schedule the timer to fire @a delta time in the future.
     * @param delta interval until expiration time
     *
     * The schedule_after methods schedule the timer relative to the current
     * system time, Timestamp::now().  When called from a timer's callback
     * function, this will usually be slightly after the timer's nominal
     * expiration time.  To schedule a timer at a strict interval,
     * compensating for small amounts of drift, use the reschedule_after
     * methods. */
    void schedule_after(const Timestamp &delta);

    /** @brief Schedule the timer to fire after @a delta_sec seconds.
     * @param delta_sec interval until expiration time, in seconds
     *
     * @sa schedule_after, reschedule_after_sec */
    inline void schedule_after_sec(uint32_t delta_sec) {
	schedule_after(Timestamp(delta_sec, 0));
    }
	
    /** @brief Schedule the timer to fire after @a delta_msec milliseconds.
     * @param delta_msec interval until expiration time, in milliseconds
     *
     * @sa schedule_after, reschedule_after_msec */
    inline void schedule_after_msec(uint32_t delta_msec) {
	schedule_after(Timestamp::make_msec(delta_msec));
    }

    /** @brief Schedule the timer to fire @a delta time after its previous
     * expiry.
     * @param delta interval until expiration time
     *
     * If the expiration time is too far in the past, then the new expiration
     * time will be silently updated to the current system time.
     *
     * @sa schedule_after */
    inline void reschedule_after(const Timestamp &delta) {
	schedule_at(_expiry + delta);
    }

    /** @brief Schedule the timer to fire @a delta_sec seconds after its
     * previous expiry.
     * @param delta_sec interval until expiration time, in seconds
     *
     * @sa schedule_after_sec, reschedule_after */
    inline void reschedule_after_sec(uint32_t delta_sec) {
	schedule_at(Timestamp(_expiry.sec() + delta_sec, _expiry.subsec()));
    }

    /** @brief Schedule the timer to fire @a delta_msec milliseconds after its
     * previous expiry.
     * @param delta_msec interval until expiration time, in milliseconds
     *
     * @sa schedule_after_msec, reschedule_after */
    inline void reschedule_after_msec(uint32_t delta_msec) {
	schedule_at(_expiry + Timestamp::make_msec(delta_msec));
    }


    /** @brief Unschedule the timer.
     *
     * The timer's expiration time is not modified. */
    void unschedule();


    /** @brief Return an adjustment interval useful for precise timers.
     *
     * Due to scheduling granularity, other tasks running on the same machine,
     * and similar effects, a Timer object can trigger some time after its
     * nominal expiry().  Functions that require precise timers should combine
     * a Timer and a Task object.  The Timer's job is to schedule the Task;
     * the Timer's expiry is set to go off a short interval before the true
     * expiry, and the Task is used to busy-wait the difference.
     * Timer::adjustment() is an appropriate value for this time
     * difference. */
    static inline Timestamp adjustment() {
	return Timestamp::make_usec(500);
    }


    /** @brief Schedule the timer to fire after @a delta_sec seconds
     * (deprecated).
     *
     * @deprecated Use schedule_after_sec() instead. */
    inline void schedule_after_s(uint32_t delta_sec) CLICK_DEPRECATED;

    /** @brief Schedule the timer to fire after @a delta_msec milliseconds
     * (deprecated).
     *
     * @deprecated Use schedule_after_msec() instead. */
    inline void schedule_after_ms(uint32_t delta_sec) CLICK_DEPRECATED;

    /** @brief Schedule the timer to fire @a delta_sec seconds after its
     * previous expiry time (deprecated).
     *
     * @deprecated Use reschedule_after_sec() instead. */
    inline void reschedule_after_s(uint32_t delta_sec) CLICK_DEPRECATED;

    /** @brief Schedule the timer to fire @a delta_msec milliseconds after its
     * previous expiry time (deprecated).
     *
     * @deprecated Use reschedule_after_msec() instead. */
    inline void reschedule_after_ms(uint32_t delta_sec) CLICK_DEPRECATED;

    enum { behind_sec = 1 };
    
  private:
  
    int _schedpos;
    Timestamp _expiry;
    TimerCallback _hook;
    void *_thunk;
    Router *_router;

    Timer(const Timer &);
    Timer &operator=(const Timer &);

    friend class Master;
  
};

inline void
Timer::schedule_after_s(uint32_t delta_sec)
{
    schedule_after_sec(delta_sec);
}

inline void
Timer::schedule_after_ms(uint32_t delta_msec)
{
    schedule_after_msec(delta_msec);
}

inline void
Timer::reschedule_after_s(uint32_t delta_sec)
{
    reschedule_after_sec(delta_sec);
}

inline void
Timer::reschedule_after_ms(uint32_t delta_msec)
{
    reschedule_after_msec(delta_msec);
}

CLICK_ENDDECLS
#endif
