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
#include <click/heap.hh>
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
     Timestamp now = Timestamp::now_steady();
     click_chatter("%s: %p{timestamp}: timer fired with expiry %p{timestamp}!\n",
                   declaration().c_str(), &now, &_timer.expiry_steady());
		   // _timer.expiry_steady() is the steady-clock Timestamp
		   // at which the timer was set to fire.
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
 Timestamp::now_steady() reports the current steady-clock time.  Note that the
 timer's expiry time goes up by exactly 5 seconds each time, and that
 steady-clock time is always later than the expiry time.

 Click aims to fire the timer as soon as possible after the expiry time, but
 cannot hit the expiry time exactly.  The reschedule_after_sec() function and
 its variants (reschedule_after(), reschedule_after_msec()) schedule the next
 firing based on the previous expiry time.  This makes the timer's action more
 robust to runtime fluctuations.  Compare:

 @code
 void PeriodicPrinter::run_timer(Timer *timer) {
     Timestamp now = Timestamp::now_steady();
     click_chatter("%s: %p{timestamp}: timer fired with expiry %p{timestamp}!\n",
                   name().c_str(), &now, &_timer.expiry_steady());
     _timer.schedule_after_sec(5);  // Fire again 5 seconds later.
         // This is the same as:
	 // _timer.schedule_at_steady(Timestamp::now_steady() + Timestamp::make_sec(5));
 }
 @endcode

 The schedule_after_sec() function sets the timer to fire an interval after
 the <em>current steady-clock time</em>, not the previous expiry.  As a
 result, the timer drifts:

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
 updated to the current steady-clock time.  Thus, the reschedule_after()
 methods will never fall more than a second or two behind steady-clock time.

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

void
Timer::do_nothing_hook(Timer *, void *)
{
}

void
Timer::element_hook(Timer *timer, void *thunk)
{
    Element* e = static_cast<Element *>(thunk);
    e->run_timer(timer);
}

void
Timer::task_hook(Timer *, void *thunk)
{
    Task* task = static_cast<Task *>(thunk);
    task->reschedule();
}


Timer::Timer()
    : _schedpos1(0), _thunk(0), _owner(0), _thread(0)
{
    static_assert(sizeof(TimerSet::heap_element) == 16, "size_element should be 16 bytes long.");
    _hook.callback = do_nothing_hook;
}

Timer::Timer(const do_nothing_t &)
    : _schedpos1(0), _thunk((void *) 1), _owner(0), _thread(0)
{
    _hook.callback = do_nothing_hook;
}

Timer::Timer(TimerCallback f, void *user_data)
    : _schedpos1(0), _thunk(user_data), _owner(0), _thread(0)
{
    _hook.callback = f;
}

Timer::Timer(Element* element)
    : _schedpos1(0), _thunk(element), _owner(0), _thread(0)
{
    _hook.callback = element_hook;
}

Timer::Timer(Task* task)
    : _schedpos1(0), _thunk(task), _owner(0), _thread(0)
{
    _hook.callback = task_hook;
}

Timer::Timer(const Timer &x)
    : _schedpos1(0), _hook(x._hook), _thunk(x._thunk), _owner(0), _thread(0)
{
}

void
Timer::initialize(Router *router)
{
    initialize(router->root_element());
}

void
Timer::initialize(Element *owner, bool quiet)
{
    assert(!initialized() || _owner->router() == owner->router());
    _owner = owner;
    if (unlikely(_hook.callback == do_nothing_hook && !_thunk) && !quiet)
	click_chatter("initializing Timer %p{element} [%p], which does nothing", _owner, this);

    int tid = owner->router()->home_thread_id(owner);
    _thread = owner->master()->thread(tid);
}

int
Timer::home_thread_id() const
{
    if (_thread)
	return _thread->thread_id();
    else
	return ThreadSched::THREAD_UNKNOWN;
}

void
Timer::schedule_at_steady(const Timestamp &when)
{
    // acquire lock, unschedule
    assert(_owner && initialized());
    TimerSet &ts = _thread->timer_set();
    ts.lock_timers();

    // set expiration timer (ensure nonzero)
    _expiry_s = when ? when : Timestamp::epsilon();
    ts.check_timer_expiry(this);

    // manipulate list; this is essentially a "decrease-key" operation
    // any reschedule removes a timer from the runchunk (XXX -- even backwards
    // reschedulings)
    int old_schedpos1 = _schedpos1;
    if (_schedpos1 <= 0) {
	if (_schedpos1 < 0)
	    ts._timer_runchunk[-_schedpos1 - 1] = 0;
	_schedpos1 = ts._timer_heap.size() + 1;
	ts._timer_heap.push_back(TimerSet::heap_element(this));
    } else
	ts._timer_heap.unchecked_at(_schedpos1 - 1).expiry_s = _expiry_s;
    change_heap<4>(ts._timer_heap.begin(), ts._timer_heap.end(),
		   ts._timer_heap.begin() + _schedpos1 - 1,
		   TimerSet::heap_less(), TimerSet::heap_place());
    if (old_schedpos1 == 1 || _schedpos1 == 1)
	ts.set_timer_expiry();

    // if we changed the timeout, wake up the thread
    if (_schedpos1 == 1)
	_thread->wake();

    // done
    ts.unlock_timers();
}

void
Timer::schedule_after(const Timestamp &delta)
{
    schedule_at_steady(Timestamp::recent_steady() + delta);
}

void
Timer::unschedule()
{
    if (!scheduled())
	return;
    TimerSet &ts = _thread->timer_set();
    ts.lock_timers();
    int old_schedpos1 = _schedpos1;
    if (_schedpos1 > 0) {
	remove_heap<4>(ts._timer_heap.begin(), ts._timer_heap.end(),
		       ts._timer_heap.begin() + _schedpos1 - 1,
		       TimerSet::heap_less(), TimerSet::heap_place());
	ts._timer_heap.pop_back();
	if (old_schedpos1 == 1)
	    ts.set_timer_expiry();
    } else if (_schedpos1 < 0)
	ts._timer_runchunk[-_schedpos1 - 1] = 0;
    _schedpos1 = 0;
    ts.unlock_timers();
}

// list-related functions in master.cc

CLICK_ENDDECLS
