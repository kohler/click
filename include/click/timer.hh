// -*- c-basic-offset: 4; related-file-name: "../../lib/timer.cc" -*-
#ifndef CLICK_TIMER_HH
#define CLICK_TIMER_HH
#include <click/sync.hh>
#include <click/glue.hh>
#include <click/element.hh>
#include <click/timestamp.hh>
CLICK_DECLS
class RouterThread;

typedef void (*TimerCallback)(Timer *timer, void *user_data);
typedef TimerCallback TimerHook CLICK_DEPRECATED;

class Timer { public:

    /** @brief Construct a Timer that does nothing when fired.
     *
     * This constructor is most useful for a Timer that will be assigned a
     * true callback later, using one of the Timer::assign() methods.
     * Timer::initialize() will report a warning if called on a Timer created
     * by this constructor. */
    Timer();

    struct do_nothing_t {
    };

    /** @brief Construct a Timer that does nothing when fired.
     *
     * Unlike with the default Timer() constructor, Timer::initialize() will
     * not report a warning if called on a Timer created by this
     * constructor. */
    Timer(const do_nothing_t &unused);

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

    /** @brief Construct a Timer that acts like @a x when fired.
     *
     * The newly-constructed Timer is not initialized. */
    Timer(const Timer &x);

    /** @brief Destroy a Timer, unscheduling it first if necessary. */
    inline ~Timer() {
	if (scheduled())
	    unschedule();
    }


    /** @brief Change the Timer to do nothing when fired. */
    inline void assign() {
	_hook.callback = do_nothing_hook;
	_thunk = (void *) 1;
    }

    /** @brief Change the Timer to do nothing when fired. */
    inline void assign(const do_nothing_t &unused) {
	(void) unused;
	assign();
    }

    /** @brief Change the Timer to call @a f(this, @a user_data) when fired.
     * @param f callback function
     * @param user_data argument for callback function */
    inline void assign(TimerCallback f, void *user_data) {
	_hook.callback = f;
	_thunk = user_data;
    }

    /** @brief Change the Timer to call @a element ->@link
     * Element::run_timer() run_timer@endlink(this) when fired.
     * @param element the element */
    void assign(Element *element) {
	_hook.callback = element_hook;
	_thunk = element;
    }

    /** @brief Change the Timer to schedule @a task when fired.
     * @param task the task */
    void assign(Task *task) {
	_hook.callback = task_hook;
	_thunk = task;
    }


    /** @brief Return true iff the Timer has been initialized. */
    inline bool initialized() const {
	return _owner != 0;
    }

    /** @brief Return true iff the Timer is currently scheduled. */
    inline bool scheduled() const {
	return _schedpos1 != 0;
    }


    /** @brief Return the Timer's steady-clock expiration time.
     *
     * This is the absolute time, according to the steady clock, at which the
     * timer is next scheduled to fire.  If the timer is not currently
     * scheduled, then expiry_steady() returns the last assigned expiration
     * time.
     *
     * @sa expiry() */
    inline const Timestamp &expiry_steady() const {
	return _expiry_s;
    }

    /** @brief Return the Timer's system-clock expiration time.
     *
     * Timer expirations are measured using the system's steady clock, which
     * increases monotonically.  (See Timestamp::now_steady().)  The expiry()
     * function, however, returns the timer's expiration time according to the
     * system clock.  This is a calculated value: if the system clock changes
     * -- because the user changes the current system time, for example --
     * then the timer's expiry() will also change.  (The timer's
     * expiry_steady() value will not change, however.)
     *
     * @sa expiry_steady() */
    inline Timestamp expiry() const {
	if (_expiry_s)
	    return _expiry_s + Timestamp::recent() - Timestamp::recent_steady();
	else
	    return _expiry_s;
    }

    /** @brief Return the Timer's associated Router. */
    inline Router *router() const {
	return _owner->router();
    }

    /** @brief Return the Timer's owning element. */
    inline Element *element() const {
	return _owner;
    }

    /** @brief Return the Timer's associated RouterThread. */
    inline RouterThread *thread() const {
	return _thread;
    }

    /** @brief Return the Timer's associated home thread ID. */
    int home_thread_id() const;


    /** @brief Initialize the timer.
     * @param owner the owner element
     * @param quiet do not produce default-constructor warning if true
     *
     * Before a timer can be used, it must be attached to a containing router.
     * When that router is destroyed, the timer is automatically
     * unscheduled.  It is safe to initialize the timer multiple times
     * on the same router.
     *
     * If Click is compiled with statistics support, time spent in this
     * Timer will be charged to the @a owner element.
     *
     * Initializing a Timer constructed by the default constructor, Timer(),
     * will produce a warning. */
    void initialize(Element *owner, bool quiet = false);

    /** @brief Initialize the timer.
     * @param router the owner router
     *
     * This function is shorthand for @link
     * Timer::initialize(Element*,bool) Timer::initialize@endlink(@a
     * router ->@link Router::root_element root_element@endlink()).
     * However, it is better to explicitly associate timers with real
     * elements. */
    void initialize(Router *router);


    /** @brief Schedule the timer to fire at @a when_steady.
     * @param when_steady expiration time according to the steady clock
     *
     * If @a when_steady is more than 2 seconds behind the current time, then
     * the expiration time is silently updated to the current time.
     *
     * @sa schedule_at() */
    void schedule_at_steady(const Timestamp &when_steady);

    /** @brief Schedule the timer to fire at @a when_steady.
     * @param when_steady expiration time according to the steady clock
     *
     * This is a synonym for schedule_at_steady(). */
    void reschedule_at_steady(const Timestamp &when_steady);

    /** @brief Schedule the timer to fire at @a when.
     * @param when expiration time according to the system clock
     *
     * If @a when is more than 2 seconds behind system time, then the
     * expiration time is silently updated to the current system time.
     *
     * @note The schedule_at_steady() function should generally be preferred
     * to schedule_at().  schedule_at() is implemented in terms of
     * schedule_at_steady().
     *
     * @sa schedule_at_steady() */
    inline void schedule_at(const Timestamp &when);

    /** @brief Schedule the timer to fire at @a when.
     * @param when expiration time
     *
     * This is a synonym for schedule_at(). */
    inline void reschedule_at(const Timestamp &when);

    /** @brief Shedule the timer to fire immediately.
     *
     * Equivalent to schedule_at(Timestamp::recent()). */
    inline void schedule_now() {
	schedule_at_steady(Timestamp::recent_steady());
    }

    /** @brief Schedule the timer to fire @a delta time in the future.
     * @param delta interval until expiration time
     *
     * The schedule_after methods schedule the timer relative to the current
     * time.  When called from a timer's callback function, this will usually
     * be slightly after the timer's nominal expiration time.  To schedule a
     * timer at a strict interval, compensating for small amounts of drift,
     * use the reschedule_after methods. */
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
	schedule_at_steady(_expiry_s + delta);
    }

    /** @brief Schedule the timer to fire @a delta_sec seconds after its
     * previous expiry.
     * @param delta_sec interval until expiration time, in seconds
     *
     * @sa schedule_after_sec, reschedule_after */
    inline void reschedule_after_sec(uint32_t delta_sec) {
	schedule_at_steady(Timestamp(_expiry_s.sec() + delta_sec, _expiry_s.subsec()));
    }

    /** @brief Schedule the timer to fire @a delta_msec milliseconds after its
     * previous expiry.
     * @param delta_msec interval until expiration time, in milliseconds
     *
     * @sa schedule_after_msec, reschedule_after */
    inline void reschedule_after_msec(uint32_t delta_msec) {
	schedule_at_steady(_expiry_s + Timestamp::make_msec(delta_msec));
    }


    /** @brief Unschedule the timer.
     *
     * The timer's expiration time is not modified. */
    void unschedule();

    /** @brief Unschedule the timer and reset its expiration time. */
    inline void clear() {
	unschedule();
	_expiry_s = Timestamp();
    }


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
#if TIMESTAMP_WARPABLE
	if (Timestamp::warp_jumping())
	    return Timestamp();
#endif
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

    int _schedpos1;
    Timestamp _expiry_s;
    union {
	TimerCallback callback;
    } _hook;
    void *_thunk;
    Element *_owner;
    RouterThread *_thread;

    Timer &operator=(const Timer &x);

    static void do_nothing_hook(Timer *t, void *user_data);
    static void element_hook(Timer *t, void *user_data);
    static void task_hook(Timer *t, void *user_data);

    friend class TimerSet;

};

inline void
Timer::schedule_at(const Timestamp &when)
{
    schedule_at_steady(when + Timestamp::recent_steady() - Timestamp::recent());
}

inline void
Timer::reschedule_at(const Timestamp &when)
{
    schedule_at(when);
}

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
