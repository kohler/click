// -*- c-basic-offset: 4; related-file-name: "../../lib/task.cc" -*-
#ifndef CLICK_TASK_HH
#define CLICK_TASK_HH
#include <click/element.hh>
#include <click/sync.hh>
#if HAVE_MULTITHREAD
# include <click/atomic.hh>
# include <click/ewma.hh>
#endif
CLICK_DECLS

#if CLICK_BSDMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <sys/lock.h>
# include <sys/mutex.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#else
#define GIANT_REQUIRED
#endif

#define PASS_GT(a, b)	((int)(a - b) > 0)

typedef bool (*TaskCallback)(Task *, void *);
typedef TaskCallback TaskHook CLICK_DEPRECATED;
class RouterThread;
class TaskList;
class Master;

struct TaskLink {
#if !HAVE_TASK_HEAP
    TaskLink *_prev;
    TaskLink *_next;
#endif
#if HAVE_STRIDE_SCHED
    unsigned _pass;
#endif
    TaskLink() {
#if !HAVE_TASK_HEAP
	_prev = _next = 0;
#endif
#if HAVE_STRIDE_SCHED
	_pass = 0;
#endif
    }
};

class Task : private TaskLink { public:

#if HAVE_STRIDE_SCHED
    enum { STRIDE1 = 1U<<16, MAX_STRIDE = 1U<<31 };
    enum { MAX_TICKETS = 1<<15, DEFAULT_TICKETS = 1<<10 };
#endif
#if HAVE_ADAPTIVE_SCHEDULER
    enum { MAX_UTILIZATION = 1000 };
#endif

    /** @brief Construct a task that calls @a f with @a user_data argument.
     *
     * @param f callback function
     * @param user_data argument for callback function
     *
     * Constructs a task that, when fired, calls @a f like so:
     *
     * @code
     * bool work_done = f(task, user_data);
     * @endcode
     *
     * where @a task is a pointer to this task.  @a f should return true if
     * the task accomplished some meaningful work, and false if it did not.
     * For example, a task that polls a network driver for packets should
     * return true if it emits at least one packet, and false if no packets
     * were available. */
    inline Task(TaskCallback f, void *user_data);

    /** @brief Construct a task that calls @a e ->@link Element::run_task(Task*) run_task()@endlink.
     *
     * @param e element to call
     *
     * Constructs a task that, when fired, calls the element @a e's @link
     * Element::run_task(Task *) run_task()@endlink method, passing this Task
     * as an argument.
     *
     * @sa Task(TaskCallback, void *) */
    inline Task(Element *e);

    /** @brief Destroy a task.
     *
     * Unschedules the task if necessary. */
    ~Task();


    /** @brief Return the task's callback function.
     *
     * Returns null if the task was constructed with the Task(Element *)
     * constructor. */
    inline TaskCallback callback() const {
	return _hook;
    }

    /** @brief Return the task callback function's user data. */
    inline void *user_data() const {
	return _thunk;
    }

    /** @brief Return the task's owning element. */
    inline Element *element() const {
	return _owner;
    }


    /** @brief Return true iff the task has been initialize()d. */
    inline bool initialized() const;

    /** @brief Return the task's home thread ID.
     *
     * This is the @link RouterThread::thread_id() thread_id()@endlink of the
     * thread on which this Task would run if it were scheduled.  This need
     * not equal the ID of the current thread(), since changes in
     * home_thread_id() aren't always implemented immediately (because of
     * locking issues). */
    inline int home_thread_id() const;

    /** @brief Return the thread on which this task is currently scheduled,
     * or would be scheduled.
     *
     * Usually, task->thread()->@link RouterThread::thread_id()
     * thread_id()@endlink == task->home_thread_id().  They can differ,
     * however, if move_thread() was called but the task hasn't yet been moved
     * to the new thread. */
    inline RouterThread *thread() const;

    /** @brief Return the router to which this task belongs. */
    inline Router *router() const {
	return _owner->router();
    }

    /** @brief Return the master where this task will be scheduled. */
    Master *master() const;


    /** @brief Initialize the Task, and optionally schedule it.
     * @param owner specifies the element owning the Task
     * @param schedule if true, the Task will be scheduled immediately
     *
     * This function must be called on every Task before it is used.  The
     * corresponding router's ThreadSched, if any, is used to determine the
     * task's initial thread assignment.  The task initially has the default
     * number of tickets, and is scheduled iff @a schedule is true.
     *
     * An assertion will fail if a Task is initialized twice.
     *
     * Most elements call ScheduleInfo::initialize_task() to initialize a Task
     * object.  The ScheduleInfo method additionally sets the task's
     * scheduling parameters, such as ticket count and thread preference,
     * based on a router's ScheduleInfo.  ScheduleInfo::initialize_task()
     * calls Task::initialize(). */
    void initialize(Element *owner, bool schedule);

    /** @brief Initialize the Task, and optionally schedule it.
     * @param router specifies the router owning the Task
     * @param schedule if true, the Task will be scheduled immediately
     *
     * This function is shorthand for @link Task::initialize(Element *, bool)
     * Task::initialize@endlink(@a router ->@link Router::root_element
     * root_element@endlink(), @a scheduled).  However, it is better to
     * explicitly associate tasks with real elements. */
    void initialize(Router *router, bool schedule);


    /** @brief Return true iff the task is currently scheduled to run.
     *
     * @note A scheduled task will usually run very soon, but not
     * always; due to locking issues, the effects of some reschedule()
     * requests may be delayed.  Also, a task unscheduled with
     * strong_unschedule() may apper scheduled(), but will not run
     * until strong_reschedule() is called. */
    inline bool scheduled() const {
	return _status.is_scheduled;
    }


    /** @brief Unschedule the task.
     *
     * After unschedule() returns, the task will not run until it is
     * rescheduled with reschedule().
     *
     * @sa reschedule, strong_unschedule */
    inline void unschedule() {
	_status.is_scheduled = false;
    }

    /** @brief Reschedule the task.
     *
     * The task is rescheduled on its home thread.  The task will eventually
     * run (unless the home thread is quiescent).
     *
     * @sa unschedule, strong_reschedule */
    inline void reschedule();

    /** @brief Reschedule a task from the task's callback function.
     *
     * @warning fast_reschedule() should be called while that task is being
     * fired: Task::fire() calls the task's callback function (often
     * Element::run_task()), which may call fast_reschedule() to reschedule
     * the task.  It is an error to call @a task.fast_reschedule() at other
     * times.  For instance, if MyElement::run_task() calls fast_reschedule(),
     * then it is an error to call MyElement::run_task() from
     * MyElement::run_timer() -- the fast_reschedule() might not actually take
     * effect.
     */
    inline void fast_reschedule() {
	_status.is_scheduled = true;
    }


    /** @brief Unschedule the Task until strong_reschedule().
     *
     * Like unschedule(), but in addition, future reschedule() calls
     * will not actually schedule the task.  Only after strong_reschedule()
     * will the task run again.
     * @sa strong_reschedule, unschedule
     */
    inline void strong_unschedule() {
	_status.is_scheduled = false;
	_status.is_strong_unscheduled = true;
    }

    /** @brief Reschedule the Task, undoing a prior strong_unschedule().
     *
     * This function undoes any previous strong_unschedule() and
     * reschedules the task.
     * @sa reschedule, strong_unschedule
     */
    inline void strong_reschedule() {
	_status.is_strong_unscheduled = false;
	reschedule();
    }


    /** @brief Move the Task to a new home thread.
     *
     * The home thread ID is set to @a new_thread_id.  The task, if it is
     * currently scheduled, is rescheduled on thread @a new_thread_id
     * (which generally takes some time to take effect).  If @a new_thread_id
     * is less than zero or greater than the number of threads on the router,
     * it is coerced to -1, and the task is scheduled on a quiescent thread
     * that never actually runs.
     */
    void move_thread(int new_thread_id);


#if HAVE_STRIDE_SCHED
    inline int tickets() const;
    inline void set_tickets(int n);
    inline void adjust_tickets(int delta);
#endif

    inline bool fire();

#if HAVE_ADAPTIVE_SCHEDULER
    inline unsigned runs() const;
    inline unsigned work_done() const;
    inline unsigned utilization() const;
    inline void clear_runs();
#endif
#if HAVE_MULTITHREAD
    inline int cycles() const;
    inline unsigned cycle_runs() const;
    inline void update_cycles(unsigned c);
#endif

    /** @cond never */
    inline TaskCallback hook() const CLICK_DEPRECATED;
    inline void *thunk() const CLICK_DEPRECATED;
    /** @endcond never */

  private:

#if HAVE_TASK_HEAP
    int _schedpos;
#endif

#if HAVE_STRIDE_SCHED
    unsigned _stride;
    int _tickets;
#endif

    union Status {
	struct {
	    int16_t home_thread_id;
	    uint8_t is_scheduled;
	    uint8_t is_strong_unscheduled;
	};
	uint32_t status;
    } _status;

    TaskCallback _hook;
    void *_thunk;

#if HAVE_ADAPTIVE_SCHEDULER
    unsigned _runs;
    unsigned _work_done;
#endif
#if HAVE_MULTITHREAD
    DirectEWMA _cycles;
    unsigned _cycle_runs;
#endif

    RouterThread *_thread;

    Element *_owner;

    union Pending {
	Task *t;
	uintptr_t x;
    };
    Pending _pending_nextptr;

    Task(const Task &x);
    Task &operator=(const Task &x);
    void cleanup();

#if CLICK_DEBUG_SCHEDULING
 public:
#endif
    inline bool on_scheduled_list() const;
    inline bool on_pending_list() const {
	return _pending_nextptr.x != 0;
    }
#if CLICK_DEBUG_SCHEDULING
 private:
#endif

    inline void add_pending_locked(RouterThread *thread);
    void add_pending();
    inline void remove_pending_locked(RouterThread *thread);
    void remove_pending();
    void process_pending(RouterThread *thread);

    inline void complete_schedule(unsigned new_pass);
    inline void fast_schedule();
    void true_reschedule();
    inline void remove_from_scheduled_list();

    static bool error_hook(Task *task, void *user_data);

    void move_thread_second_half();

    friend class RouterThread;
    friend class Master;

};


// need RouterThread's definition for inline functions
CLICK_ENDDECLS
#include <click/routerthread.hh>
CLICK_DECLS


inline
Task::Task(TaskCallback f, void *user_data)
    :
#if HAVE_TASK_HEAP
      _schedpos(-1),
#endif
#if HAVE_STRIDE_SCHED
      _stride(0), _tickets(-1),
#endif
      _hook(f), _thunk(user_data),
#if HAVE_ADAPTIVE_SCHEDULER
      _runs(0), _work_done(0),
#endif
#if HAVE_MULTITHREAD
      _cycle_runs(0),
#endif
      _thread(0), _owner(0)
{
    _status.home_thread_id = -1;
    _status.is_scheduled = _status.is_strong_unscheduled = false;
    _pending_nextptr.x = 0;
}

inline
Task::Task(Element* e)
    :
#if HAVE_TASK_HEAP
      _schedpos(-1),
#endif
#if HAVE_STRIDE_SCHED
      _stride(0), _tickets(-1),
#endif
      _hook(0), _thunk(e),
#if HAVE_ADAPTIVE_SCHEDULER
      _runs(0), _work_done(0),
#endif
#if HAVE_MULTITHREAD
      _cycle_runs(0),
#endif
      _thread(0), _owner(0)
{
    _status.home_thread_id = -1;
    _status.is_scheduled = _status.is_strong_unscheduled = false;
    _pending_nextptr.x = 0;
}

inline bool
Task::initialized() const
{
    return _owner != 0;
}

inline bool
Task::on_scheduled_list() const
{
#if HAVE_TASK_HEAP
    return _schedpos >= 0;
#else
    return _prev != 0;
#endif
}

/** @cond never */
/** @brief Return the task's callback function.
 * @deprecated Use callback() instead. */
inline TaskCallback
Task::hook() const
{
    return _hook;
}

/** @brief Return the task's callback data.
 * @deprecated Use user_data() instead. */
inline void *
Task::thunk() const
{
    return _thunk;
}
/** @endcond never */

inline int
Task::home_thread_id() const
{
    return _status.home_thread_id;
}

inline RouterThread *
Task::thread() const
{
    return _thread;
}

inline void
Task::remove_from_scheduled_list()
{
    if (on_scheduled_list()) {
#if HAVE_TASK_HEAP
	Task *back = _thread->_task_heap.back().t;
	_thread->_task_heap.pop_back();
	if (_thread->_task_heap.size() > 0)
	    _thread->task_reheapify_from(_schedpos, back);
	_schedpos = -1;
#else
	_next->_prev = _prev;
	_prev->_next = _next;
	_next = _prev = 0;
#endif
    }
}

#if HAVE_STRIDE_SCHED

/** @brief Return the task's number of tickets.
 *
 * Tasks with larger numbers of tickets are scheduled more often.  Tasks are
 * initialized with tickets() == DEFAULT_TICKETS.
 *
 * @sa set_tickets, adjust_tickets
 */
inline int
Task::tickets() const
{
    return _tickets;
}

/** @brief Set the task's ticket count.
 * @param n the ticket count
 *
 * The ticket count @a n is pinned to the range [1, MAX_TICKETS].
 *
 * @sa tickets, adjust_tickets
 */
inline void
Task::set_tickets(int n)
{
    if (n > MAX_TICKETS)
	n = MAX_TICKETS;
    else if (n < 1)
	n = 1;
    _tickets = n;
    _stride = STRIDE1 / n;
    assert(_stride < MAX_STRIDE);
}

/** @brief Add @a delta to the Task's ticket count.
 * @param delta adjustment to the ticket count
 *
 * The ticket count cannot be adjusted below 1 or above MAX_TICKETS.
 *
 * @sa set_tickets
 */
inline void
Task::adjust_tickets(int delta)
{
    set_tickets(_tickets + delta);
}

#endif /* HAVE_STRIDE_SCHED */


inline void
Task::complete_schedule(unsigned new_pass)
{
    assert(_thread && !on_scheduled_list());
#if CLICK_LINUXMODULE
    // tasks never run at interrupt time in Linux
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    GIANT_REQUIRED;
#endif

#if HAVE_STRIDE_SCHED
    // update pass
    _pass = new_pass;

# if HAVE_TASK_HEAP
    _schedpos = _thread->_task_heap.size();
    _thread->_task_heap.push_back(RouterThread::task_heap_element());
    _thread->task_reheapify_from(_schedpos, this);
# elif 0
    // look for 'n' immediately before where we should be scheduled
    TaskLink *n = _thread->_prev;
    while (n != _thread && PASS_GT(n->_pass, _pass))
	n = n->_prev;
    // schedule after 'n'
    _next = n->_next;
    _prev = n;
    n->_next = this;
    _next->_prev = this;
# else
    // look for 'n' immediately after where we should be scheduled
    TaskLink *n = _thread->_task_link._next;
    while (n != &_thread->_task_link && !PASS_GT(n->_pass, _pass))
	n = n->_next;
    // schedule before 'n'
    _prev = n->_prev;
    _next = n;
    n->_prev = this;
    _prev->_next = this;
# endif

#else /* !HAVE_STRIDE_SCHED */
    (void) new_pass;

    // schedule at the end of the list
    _prev = _thread->_task_link._prev;
    _next = &_thread->_task_link;
    _thread->_task_link._prev = this;
    _prev->_next = this;
#endif /* HAVE_STRIDE_SCHED */
}

inline void
Task::fast_schedule()
{
    if (!on_scheduled_list()) {
#if HAVE_STRIDE_SCHED
	assert(_tickets >= 1);
	complete_schedule(_thread->pass() + _stride);
#else
	complete_schedule(0);
#endif
    }
}

inline void
Task::reschedule()
{
    _status.is_scheduled = true;
    if (!on_scheduled_list()
	|| (likely(_thread) && !_thread->current_thread_is_running()))
	true_reschedule();
}


/** @brief Fire the task by calling its callback function.
 *
 * This function is generally called by the RouterThread implementation; there
 * should be no need to call it yourself.
 */
inline bool
Task::fire()
{
#if CLICK_STATS >= 2
    click_cycles_t start_cycles = click_get_cycles(),
	start_child_cycles = _owner->_child_cycles;
#endif
#if HAVE_MULTITHREAD
    _cycle_runs++;
#endif
    bool work_done;
    if (!_hook)
	work_done = ((Element*)_thunk)->run_task(this);
    else
	work_done = _hook(this, _thunk);
#if HAVE_ADAPTIVE_SCHEDULER
    ++_runs;
    _work_done += work_done;
#endif
#if CLICK_STATS >= 2
    click_cycles_t all_delta = click_get_cycles() - start_cycles,
	own_delta = all_delta - (_owner->_child_cycles - start_child_cycles);
    _owner->_task_calls += 1;
    _owner->_task_own_cycles += own_delta;
#endif
    return work_done;
}

#if HAVE_ADAPTIVE_SCHEDULER
inline unsigned
Task::runs() const
{
    return _runs;
}

inline unsigned
Task::work_done() const
{
    return _work_done;
}

inline unsigned
Task::utilization() const
{
    return (_runs ? (MAX_UTILIZATION * _work_done) / _runs : 0);
}

inline void
Task::clear_runs()
{
    _runs = _work_done = 0;
}
#endif

#if HAVE_MULTITHREAD
inline int
Task::cycles() const
{
    return _cycles.unscaled_average();
}

inline unsigned
Task::cycle_runs() const
{
    return _cycle_runs;
}

inline void
Task::update_cycles(unsigned c)
{
    _cycles.update(c);
    _cycle_runs = 0;
}
#endif

CLICK_ENDDECLS
#endif
