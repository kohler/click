// -*- c-basic-offset: 4; related-file-name: "../include/click/timer.hh" -*-
/*
 * timer.{cc,hh} -- portable timers
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/router.hh>
#include <click/master.hh>
#include <click/routerthread.hh>
#include <click/task.hh>
CLICK_DECLS

/** @file timer.hh
 * @brief Support for Timer, which triggers execution at a given time.
 */

/** @class Timer
 @brief Triggers execution at a given time.

 Click Timer objects trigger the execution of code after a specific time.
 Click's version of "ping" (the ICMPPingSource element) uses Timer objects to
 create ping packets at specific times.  Many other elements, such as
 IPRewriter, garbage-collect their internal state based on timers.  An element
 that needs to run occasional timed tasks includes and initializes a Timer
 instance variable.  When scheduled, most timers call their associated
 element's @link Element::run_timer() run_timer()@endlink method.

 Each scheduled Timer has a single expiration Timestamp.  To implement a
 periodic timer, reschedule the timer as appropriate.

 <h3>Examples</h3>
 
 This example element code, based on TimedSource, will print a message every
 5 seconds:

 @code
 #include <click/element.hh>
 #include <click/timer.hh>
 
 class PeriodicPrinter : public Element { public:
     PeriodicPrinter();
     const char *class_name() const { return "PeriodicPrinter"; }
     int initialize(ErrorHandler *errh);
     void run_timer(Timer *timer);
   private:
     Timer _timer;
 };

 PeriodicPrinter::PeriodicPrinter()
     : _timer(this)    // Sets _timer to call this->run_timer(&_timer)
 {                     // when it fires.
 }

 int PeriodicPrinter::initialize(ErrorHandler *) {
     _timer.initialize(this);   // Initialize timer object (mandatory).
     _timer.schedule_now();     // Set the timer to fire as soon as the
                                // router runs.
     return 0;
 }

 void PeriodicPrinter::run_timer(Timer *timer) {
     // This function is called when the timer fires.
     assert(timer == &_timer);
     Timestamp now = Timestamp::now();
     click_chatter("%s: %{timestamp}: timer fired with expiry %{timestamp}!\n",
                   declaration().c_str(), &now, &_timer.expiry());
		   // _timer.expiry() is the Timestamp at which the timer
		   // was set to fire.
     _timer.reschedule_after_sec(5);  // Fire again 5 seconds later.
 }
 @endcode

 Running this element might produce output like this:

 <pre>
 pp: 1204658365.127870: timer fired with expiry 1204658365.127847!
 pp: 1204658370.127911: timer fired with expiry 1204658370.127847!
 pp: 1204658375.127877: timer fired with expiry 1204658375.127847!
 pp: 1204658380.127874: timer fired with expiry 1204658380.127847!
 pp: 1204658385.127876: timer fired with expiry 1204658385.127847!
 pp: 1204658390.127926: timer fired with expiry 1204658390.127847!
 pp: 1204658395.128044: timer fired with expiry 1204658395.127847!
 </pre>

 The expiry time measures when the timer was supposed to fire, while
 Timestamp::now() reports the current system time.  Note that the timer's
 expiry time goes up by exactly 5 seconds each time, and that system time
 is always later than the expiry time.

 Click aims to fire the timer as soon as possible after the expiry time, but
 cannot hit the expiry time exactly.  The reschedule_after_sec() function and
 its variants (reschedule_after(), reschedule_after_msec()) schedule the next
 firing based on the previous expiry time.  This makes the timer's action more
 robust to runtime fluctuations.  Compare:

 @code
 void PeriodicPrinter::run_timer(Timer *timer) {
     Timestamp now = Timestamp::now();
     click_chatter("%s: %{timestamp}: timer fired with expiry %{timestamp}!\n",
                   name().c_str(), &now, &_timer.expiry());
     _timer.schedule_after_sec(5);  // Fire again 5 seconds later.
         // This is the same as:
	 // _timer.schedule_at(Timestamp::now() + Timestamp::make_sec(5));
 }
 @endcode 

 The schedule_after_sec() function sets the timer to fire an interval after
 the <em>current system time</em>, not the previous expiry.  As a result, the
 timer drifts:
 
 <pre>
 pp: 1204658494.374277: timer fired with expiry 1204658494.374256!
 pp: 1204658499.374575: timer fired with expiry 1204658499.374478!
 pp: 1204658504.375261: timer fired with expiry 1204658504.375218!
 pp: 1204658509.375428: timer fired with expiry 1204658509.375381!
 ...
 pp: 1204658884.998112: timer fired with expiry 1204658884.998074!
 pp: 1204658890.001909: timer fired with expiry 1204658889.998209!
 pp: 1204658895.002399: timer fired with expiry 1204658895.002175!
 pp: 1204658900.003626: timer fired with expiry 1204658900.003589!
 </pre>

 Timers that are set to fire more than 1 second in the past are silently
 updated to the current system time.  Thus, the reschedule_after() methods
 will never fall more than a second or two behind system time.

 <h3>Notes</h3>
 
 Elements desiring extremely frequent access to the CPU, up to tens of
 thousands of times a second, should use a Task object rather than a Timer.
 However, Tasks essentially busy-wait, taking up all available CPU.  There is
 a tradeoff, and some elements combine a Task and a Timer to get the benefits
 of both; for example, LinkUnqueue uses a Task at high rates and a Timer at
 low rates.  The Timer::adjustment() value is useful in this context.

 Particularly at user level, there can be a significant delay between a
 Timer's nominal expiration time and the actual time it runs.  Elements that
 desire extremely precise timings should combine a Timer with a Task.  The
 Timer is set to go off a bit before the true expiration time (see
 Timer::adjustment()), after which the Task polls the CPU until the actual
 expiration time arrives.
 
 Since Click is cooperatively scheduled, any timer callback should run for
 just a short period of time.  Very long callbacks can inappropriately delay
 other timers and periodic events.

 The Click core stores timers in a heap, so most timer operations (including
 scheduling and unscheduling) take @e O(log @e n) time and Click can handle
 very large numbers of timers.

 Timers generally run in increasing order by expiration time.  That is, if
 timer @a a's expiry() is less than timer @a b's expiry(), then @a a will
 generally fire before @a b.  However, Click must sometimes run timers out of
 order to ensure fairness.  The only strict guarantee is that a Timer will run
 after its nominal expiration time.
*/

static void
empty_hook(Timer *, void *)
{
}

static void
element_hook(Timer *timer, void *thunk)
{
    Element* e = static_cast<Element *>(thunk);
    e->run_timer(timer);
}

static void
task_hook(Timer *, void *thunk)
{
    Task* task = static_cast<Task *>(thunk);
    task->reschedule();
}


/** @brief Create a Timer that will do nothing when fired.
 */
Timer::Timer()
    : _schedpos(-1), _hook(empty_hook), _thunk(0), _router(0)
{
}

/** @brief Create a Timer that will call @a hook(this, @a thunk) when fired.
 *
 * @param hook the callback function
 * @param thunk argument for the callback function
 */
Timer::Timer(TimerHook hook, void* thunk)
    : _schedpos(-1), _hook(hook), _thunk(thunk), _router(0)
{
}

/** @brief Create a Timer that will call @a element ->@link
 * Element::run_timer() run_timer@endlink(this) when fired.
 *
 * @param element the element
 */
Timer::Timer(Element* element)
    : _schedpos(-1), _hook(element_hook), _thunk(element), _router(0)
{
}

/** @brief Create a Timer that will schedule @a task when fired.
 *
 * @param task the task
 */
Timer::Timer(Task* task)
    : _schedpos(-1), _hook(task_hook), _thunk(task), _router(0)
{
}

/** @brief Change the Timer to call @a hook(this, @a thunk) when fired.
 *
 * @param hook the callback function
 * @param thunk argument for the callback function
 */
void Timer::set_hook(TimerHook hook, void* thunk)
{
    _hook = hook;
    _thunk = thunk;
}

/** @brief Change the Timer to call @a element ->@link
 * Element::run_timer() run_timer@endlink(this) when fired.
 *
 * @param element the element
 */
void Timer::set_hook(Element* element)
{
    _hook = element_hook;
    _thunk = element;
}

/** @brief Change the Timer to schedule @a task when fired.

 * @param task the task
 */
void Timer::set_hook(Task* task)
{
    _hook = task_hook;
    _thunk = task;
}

/** @brief Schedule the timer to fire at @a when.
 *
 * @param when expiration time
 *
 * If @a when is more than 2 seconds behind system time, then the expiration
 * time is silently updated to the current system time.
 */
void
Timer::schedule_at(const Timestamp& when)
{
    // acquire lock, unschedule
    assert(_router && initialized());
    Master* master = _router->master();
    master->lock_timers();

    // set expiration timer
    _expiry = when;

    // manipulate list; this is essentially a "decrease-key" operation
    if (!scheduled()) {
	_schedpos = master->_timer_heap.size();
	master->_timer_heap.push_back(0);
    }
    master->timer_reheapify_from(_schedpos, this, false);

    // if we changed the timeout, wake up the first thread
    if (_schedpos == 0)
	master->_threads[2]->wake();

    // done
    master->unlock_timers();
}

/** @brief Schedule the timer to fire @a delta time in the future.
 *
 * @param delta interval until expiration time
 *
 * The schedule_after methods schedule the timer relative to the current
 * system time, Timestamp::now().  When called from a timer's expiration hook,
 * this will usually be slightly after the timer's nominal expiration time.
 * To schedule a timer at a strict interval, compensating for small amounts of
 * drift, use the reschedule_after methods.
 */
void
Timer::schedule_after(const Timestamp &delta)
{
    schedule_at(Timestamp::now() + delta);
}

/** @brief Unschedule the timer.
 *
 * The timer's expiration time is not modified.
 */
void
Timer::unschedule()
{
    if (!scheduled())
	return;
    Master* master = _router->master();
    master->lock_timers();
    if (scheduled()) {
	master->timer_reheapify_from(_schedpos, master->_timer_heap.back(), true);
	_schedpos = -1;
	master->_timer_heap.pop_back();
    }
    master->unlock_timers();
}

// list-related functions in master.cc

CLICK_ENDDECLS
