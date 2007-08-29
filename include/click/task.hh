// -*- c-basic-offset: 4; related-file-name: "../../lib/task.cc" -*-
#ifndef CLICK_TASK_HH
#define CLICK_TASK_HH
#include <click/element.hh>
#include <click/sync.hh>
#if __MTCLICK__
# include <click/atomic.hh>
# include <click/ewma.hh>
#endif
CLICK_DECLS

#if CLICK_BSDMODULE
extern "C" {
 #include <sys/lock.h>
 #include <sys/mutex.h>
}
#else
#define GIANT_REQUIRED
#endif

#define PASS_GT(a, b)	((int)(a - b) > 0)

typedef bool (*TaskHook)(Task *, void *);
class RouterThread;
class TaskList;
class Master;

class Task { public:

#ifdef HAVE_STRIDE_SCHED
    enum { STRIDE1 = 1U<<16, MAX_STRIDE = 1U<<31 };
    enum { MAX_TICKETS = 1<<15, DEFAULT_TICKETS = 1<<10 };
#endif
#ifdef HAVE_ADAPTIVE_SCHEDULER
    enum { MAX_UTILIZATION = 1000 };
#endif

    inline Task(TaskHook hook, void *thunk);
    inline Task(Element *e);		// call element->run_task()
    ~Task();

    inline TaskHook hook() const;
    inline void *thunk() const;
    inline Element *element() const;

    inline bool scheduled() const;

    void unschedule();
    inline void reschedule();

    void strong_unschedule();
    void strong_reschedule();

#ifdef HAVE_TASK_HEAP
    void fast_reschedule();
#else
    inline void fast_reschedule();
#endif

    inline int home_thread_id() const;
    void move_thread(int thread_id);
 
#ifdef HAVE_STRIDE_SCHED
    inline int tickets() const;
    inline void set_tickets(int n);
    inline void adjust_tickets(int delta);
#endif

    inline bool initialized() const;
    void initialize(Router *, bool scheduled);
    void initialize(Element *, bool scheduled);
    inline RouterThread *thread() const;
    Master *master() const;

    inline void call_hook();

#ifdef HAVE_ADAPTIVE_SCHEDULER
    inline unsigned runs() const;
    inline unsigned work_done() const;
    inline unsigned utilization() const;
    inline void clear_runs();
#endif
#if __MTCLICK__
    inline int cycles() const;
    inline unsigned cycle_runs() const;
    inline void update_cycles(unsigned c);
#endif

  private:

    /* if gcc keeps this ordering, we may get some cache locality on a 16 or 32
     * byte cache line: the first three fields are used in list traversal */

#ifdef HAVE_TASK_HEAP
    int _schedpos;
#else
    Task* _prev;
    Task* _next;
#endif
    
#ifdef HAVE_STRIDE_SCHED
    unsigned _pass;
    unsigned _stride;
    int _tickets;
#endif
  
    TaskHook _hook;
    void* _thunk;
  
#ifdef HAVE_ADAPTIVE_SCHEDULER
    unsigned _runs;
    unsigned _work_done;
#endif
#if __MTCLICK__
    DirectEWMA _cycles;
    unsigned _cycle_runs;
#endif

    RouterThread* _thread;
    int _home_thread_id;
  
    Router* _router;

    unsigned _pending_reschedule;
    uintptr_t _pending_nextptr;

    Task(const Task&);
    Task& operator=(const Task&);
    void cleanup();
    
    void add_pending(bool reschedule);
    void process_pending(RouterThread*);
    inline void fast_schedule();
    void true_reschedule();
    inline void lock_tasks();
    inline bool attempt_lock_tasks();

    static bool error_hook(Task*, void*);

    inline void fast_unschedule();

    static inline Task *pending_to_task(uintptr_t);
    inline Task *pending_to_task() const;
    
    friend class RouterThread;
    friend class Master;
  
};


// need RouterThread's definition for inline functions
CLICK_ENDDECLS
#include <click/routerthread.hh>
CLICK_DECLS


/** @brief Construct a task that calls @a hook with @a thunk argument.
 *
 * @param hook task hook
 * @param thunk user data for task hook
 *
 * Constructs a task that, when fired, calls the @a hook function like so:
 *
 * @code
 * bool work_done = hook(task, thunk);
 * @endcode
 *
 * where @a task is a pointer to this task.  The @a hook should return true if
 * the task accomplished some meaningful work, and false if it did not.  For
 * example, a task that polls a network driver for packets should return true
 * if it emits at least one packet, and false if no packets were available.
 */
inline
Task::Task(TaskHook hook, void* thunk)
#ifdef HAVE_TASK_HEAP
    : _schedpos(-1),
#else
    : _prev(0), _next(0),
#endif
#ifdef HAVE_STRIDE_SCHED
      _pass(0), _stride(0), _tickets(-1),
#endif
      _hook(hook), _thunk(thunk),
#ifdef HAVE_ADAPTIVE_SCHEDULER
      _runs(0), _work_done(0),
#endif
#if __MTCLICK__
      _cycle_runs(0),
#endif
      _thread(0), _home_thread_id(-1),
      _router(0), _pending_reschedule(0), _pending_nextptr(0)
{
}

/** @brief Construct a task that calls @a e ->@link Element::run_task(Task *)
 * run_task()@endlink.
 *
 * @param e element to call
 *
 * Constructs a task that, when fired, calls the element @a e's @link
 * Element::run_task(Task *) run_task()@endlink method, passing this Task
 * as an argument.
 *
 * @sa Task(TaskHook, void *)
 */
inline
Task::Task(Element* e)
#ifdef HAVE_TASK_HEAP
    : _schedpos(-1),
#else
    : _prev(0), _next(0),
#endif
#ifdef HAVE_STRIDE_SCHED
      _pass(0), _stride(0), _tickets(-1),
#endif
      _hook(0), _thunk(e),
#ifdef HAVE_ADAPTIVE_SCHEDULER
      _runs(0), _work_done(0),
#endif
#if __MTCLICK__
      _cycle_runs(0),
#endif
      _thread(0), _home_thread_id(-1),
      _router(0), _pending_reschedule(0), _pending_nextptr(0)
{
}

/** @brief Returns true iff the task has been initialize()d. */
inline bool
Task::initialized() const
{
    return _router != 0;
}

/** @brief Returns true iff the task is currently scheduled to run.
 *
 * This function will return false for a task in reschedule-pending state,
 * where the task will soon be rescheduled but isn't right now due to locking
 * issues.
 */
inline bool
Task::scheduled() const
{
#if HAVE_TASK_HEAP
    return _schedpos >= 0;
#else
    return _prev != 0;
#endif
}

/** @brief Returns the task's hook function.
 *
 * Returns null if the task was constructed with the Task(Element *)
 * constructor.
 */
inline TaskHook
Task::hook() const
{
    return _hook;
}

/** @brief Returns the task's thunk (the user data passed to its hook).
 */
inline void *
Task::thunk() const
{
    return _thunk;
}

/** @brief Returns the task's associated element, if any.
 *
 * Only works if the task was constructed with the Task(Element *)
 * constructor.
 */
inline Element *
Task::element()	const
{ 
    return _hook ? 0 : reinterpret_cast<Element*>(_thunk); 
}

/** @brief Returns the task's home thread ID.
 *
 * This is the @link RouterThread::thread_id() thread_id()@endlink of the
 * thread on which this Task would run if it were scheduled.  This need not
 * equal the ID of the current thread(), since changes in home_thread_id()
 * aren't always implemented immediately (because of locking issues).
 */
inline int
Task::home_thread_id() const
{
    return _home_thread_id;
}

/** @brief Returns the thread on which this element is currently scheduled, or
 * would be scheduled.
 *
 * Usually, task->thread()->@link RouterThread::thread_id()
 * thread_id()@endlink == task->home_thread_id().  They can differ, however,
 * if move_thread() was called but the task hasn't yet been moved to
 * the new thread, or if the task was strongly unscheduled with
 * strong_unschedule().  (In this last case, task->thread()->@link
 * RouterThread::thread_id() thread_id()@endlink ==
 * RouterThread::THREAD_STRONG_UNSCHEDULE.)
 */
inline RouterThread *
Task::thread() const
{
    return _thread;
}

inline void
Task::fast_unschedule()
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    GIANT_REQUIRED;
#endif
    if (scheduled()) {
#ifdef HAVE_TASK_HEAP
	Task* back = _thread->_task_heap.back();
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

#ifdef HAVE_STRIDE_SCHED

/** @brief Returns the task's number of tickets.
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

/** @brief Sets the task's number of tickets.
 * @param n the ticket count
 *
 * The ticket count @a n is pinned in the range [1, MAX_TICKETS].
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

# ifndef HAVE_TASK_HEAP
/** @brief Reschedules the task.  The task's current thread must be currently
 * locked.
 *
 * This accomplishes the same function as reschedule(), but does a faster job
 * because it assumes the task's thread lock is held.  Generally, this can be
 * guaranteed only from within a task's run_task() hook function.
 */
inline void
Task::fast_reschedule()
{
    assert(_thread);
#  if CLICK_LINUXMODULE
    // tasks never run at interrupt time in Linux
    assert(!in_interrupt());
#  endif
#  if CLICK_BSDMODULE
    GIANT_REQUIRED;
#  endif

    if (!scheduled()) {
	// increase pass
	_pass += _stride;

#  if 0
	// look for 'n' immediately before where we should be scheduled
	Task* n = _thread->_prev;
	while (n != _thread && PASS_GT(n->_pass, _pass))
	    n = n->_prev;

	// schedule after 'n'
	_next = n->_next;
	_prev = n;
	n->_next = this;
	_next->_prev = this;
#  else
	// look for 'n' immediately after where we should be scheduled
	Task* n = _thread->_next;
#   ifdef CLICK_BSDMODULE /* XXX MARKO a race occured here when not spl'ed */
	while (n->_next != NULL && n != _thread && !PASS_GT(n->_pass, _pass))
#   else
	while (n != _thread && !PASS_GT(n->_pass, _pass))
#   endif
	    n = n->_next;
    
	// schedule before 'n'
	_prev = n->_prev;
	_next = n;
	_prev->_next = this;
	n->_prev = this;
#  endif
    }
}
# endif /* !HAVE_TASK_HEAP */

inline void
Task::fast_schedule()
{
    GIANT_REQUIRED;
    assert(_tickets >= 1);
    _pass = _thread->_pass;
    fast_reschedule();
}

#else /* !HAVE_STRIDE_SCHED */

inline void
Task::fast_reschedule()
{
    assert(_thread);
#if CLICK_LINUXMODULE
    // tasks never run at interrupt time
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    // assert(!intr_nesting_level);
    GIANT_REQUIRED;
#endif
    if (!scheduled()) {
	_prev = _thread->_prev;
	_next = _thread;
	_thread->_prev = this;
	_thread->_next = this;
    }
}

inline void
Task::fast_schedule()
{
    fast_reschedule();
}

#endif /* HAVE_STRIDE_SCHED */


/** @brief Reschedules the task.
 *
 * The task is rescheduled on its home thread.  The task will eventually run
 * (unless the home thread is quiescent).  Due to locking issues, the task may
 * not be scheduled right away -- scheduled() may not immediately return true.
 *
 * @sa unschedule, strong_reschedule
 */
inline void
Task::reschedule()
{
    GIANT_REQUIRED;
    if (!scheduled())
	true_reschedule();
}


/** @brief Call the task's hook.
 *
 * This function is generally called by the RouterThread implementation; there
 * should be no need to call it yourself.
 */
inline void
Task::call_hook()
{
#if __MTCLICK__
    _cycle_runs++;
#endif
#ifdef HAVE_ADAPTIVE_SCHEDULER
    _runs++;
    if (!_hook)
	_work_done += ((Element*)_thunk)->run_task(this);
    else
	_work_done += _hook(this, _thunk);
#else
    if (!_hook)
	(void) ((Element*)_thunk)->run_task(this);
    else
	(void) _hook(this, _thunk);
#endif
}

#ifdef HAVE_ADAPTIVE_SCHEDULER
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

#if __MTCLICK__
inline int
Task::cycles() const
{
    return _cycles.average() >> _cycles.scale;
}

inline unsigned
Task::cycle_runs() const
{
    return _cycle_runs;
}

inline void
Task::update_cycles(unsigned c) 
{
    _cycles.update_with(c);
    _cycle_runs = 0;
}
#endif

inline Task *
Task::pending_to_task(uintptr_t ptr)
{
    return reinterpret_cast<Task *>(ptr & ~(uintptr_t) 3);
}

inline Task *
Task::pending_to_task() const
{
    return pending_to_task(_pending_nextptr);
}

CLICK_ENDDECLS
#endif
