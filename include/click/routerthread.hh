// -*- c-basic-offset: 4; related-file-name: "../../lib/routerthread.cc" -*-
#ifndef CLICK_ROUTERTHREAD_HH
#define CLICK_ROUTERTHREAD_HH
#include <click/sync.hh>
#include <click/vector.hh>
#include <click/timerset.hh>
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/sched.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#elif CLICK_BSDMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <sys/systm.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#elif CLICK_USERLEVEL
# include <click/selectset.hh>
#endif

// NB: user must #include <click/task.hh> before <click/routerthread.hh>.
// We cannot #include <click/task.hh> ourselves because of circular #include
// dependency.
CLICK_DECLS

class RouterThread { public:

    enum { THREAD_QUIESCENT = -1, THREAD_UNKNOWN = -1000 };

    inline int thread_id() const;

    inline Master *master() const;
    inline TimerSet &timer_set()		{ return _timers; }
    inline const TimerSet &timer_set() const	{ return _timers; }
#if CLICK_USERLEVEL
    inline SelectSet &select_set()		{ return _selects; }
    inline const SelectSet &select_set() const	{ return _selects; }
#endif

    // Task list functions
    inline bool active() const;
    inline Task *task_begin() const;
    inline Task *task_next(Task *task) const;
    inline Task *task_end() const;
    void scheduled_tasks(Router *router, Vector<Task *> &x);

    inline void lock_tasks();
    inline void unlock_tasks();

    inline void schedule_block_tasks();
    inline void block_tasks(bool scheduled);
    inline void unblock_tasks();

    inline bool stop_flag() const;

    void driver();
    void driver_once();

    void kill_router(Router *router);

#if HAVE_ADAPTIVE_SCHEDULER
    // min_cpu_share() and max_cpu_share() are expressed on a scale with
    // Task::MAX_UTILIZATION == 100%.
    unsigned min_cpu_share() const	{ return _min_click_share; }
    unsigned max_cpu_share() const	{ return _max_click_share; }
    unsigned cur_cpu_share() const	{ return _cur_click_share; }
    void set_cpu_share(unsigned min_share, unsigned max_share);
#endif

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
    bool greedy() const			{ return _greedy; }
    void set_greedy(bool g)		{ _greedy = g; }
#endif

    inline void wake();

#if CLICK_USERLEVEL
    inline void run_signals();
#endif

    enum { S_PAUSED, S_BLOCKED, S_TIMERWAIT,
	   S_LOCKSELECT, S_LOCKTASKS,
	   S_RUNTASK, S_RUNTIMER, S_RUNSIGNAL, S_RUNPENDING, S_RUNSELECT,
	   NSTATES };
    inline void set_thread_state(int state);
    inline void set_thread_state_for_blocking(int delay_type);
#if CLICK_DEBUG_SCHEDULING
    int thread_state() const		{ return _thread_state; }
    static String thread_state_name(int state);
    uint32_t driver_epoch() const	{ return _driver_epoch; }
    uint32_t driver_task_epoch() const	{ return _driver_task_epoch; }
    Timestamp task_epoch_time(uint32_t epoch) const;
# if CLICK_LINUXMODULE
    struct task_struct *sleeper() const	{ return _linux_task; }
# endif
# if CLICK_DEBUG_SCHEDULING > 1
    inline Timestamp thread_state_time(int state) const;
    inline uint64_t thread_state_count(int state) const;
# endif
#endif

  private:

#if HAVE_TASK_HEAP
    struct task_heap_element {
	unsigned pass;
	Task *t;
	task_heap_element() {
	}
	task_heap_element(Task *t_)
	    : pass(t_->_pass), t(t_) {
	}
    };
#endif

    // LOCAL STATE GROUP
    TaskLink _task_link;
    volatile int _stop_flag;
#if HAVE_TASK_HEAP
    Vector<task_heap_element> _task_heap;
#endif

    TimerSet _timers;
#if CLICK_USERLEVEL
    SelectSet _selects;
#endif

#if HAVE_ADAPTIVE_SCHEDULER
    enum { C_CLICK, C_KERNEL, NCLIENTS };
    struct Client {			// top-level stride clients
	unsigned pass;
	unsigned stride;
	int tickets;
	Client() : pass(0), tickets(0)	{ }
    };
    Client _clients[NCLIENTS];
    unsigned _global_pass;		// global pass
    unsigned _max_click_share;		// maximum allowed Click share of CPU
    unsigned _min_click_share;		// minimum allowed Click share of CPU
    unsigned _cur_click_share;		// current Click share
    Timestamp _adaptive_restride_timestamp;
    int _adaptive_restride_iter;
#endif

    // EXTERNAL STATE GROUP
    Spinlock _task_lock CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
    atomic_uint32_t _task_blocker;
    atomic_uint32_t _task_blocker_waiting;

    uintptr_t _pending_head;
    volatile uintptr_t *_pending_tail;
    SpinlockIRQ _pending_lock;

    // SHARED STATE GROUP
    Master *_master CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
    int _id;
#if HAVE_MULTITHREAD && !CLICK_LINUXMODULE
    click_processor_t _running_processor;
#endif
#if CLICK_LINUXMODULE
    struct task_struct *_linux_task;
    bool _greedy;
#endif
  public:
    unsigned _tasks_per_iter;
    unsigned _iters_per_os;
  private:

#if CLICK_NS
    Timestamp _ns_scheduled;
    Timestamp _ns_last_active;
    int _ns_active_iter;
    enum { ns_iters_per_time = 1000 };
#endif

#if CLICK_BSDMODULE
    // XXX FreeBSD
    u_int64_t _old_tsc; /* MARKO - temp. */
    void *_sleep_ident;
    int _oticks;
    bool _greedy;
#endif

#if CLICK_DEBUG_SCHEDULING
    int _thread_state;
    uint32_t _driver_epoch;
    uint32_t _driver_task_epoch;
    enum { TASK_EPOCH_BUFSIZ = 32 };
    uint32_t _task_epoch_first;
    Timestamp _task_epoch_time[TASK_EPOCH_BUFSIZ];
# if CLICK_DEBUG_SCHEDULING > 1
    Timestamp _thread_state_time[NSTATES];
    uint64_t _thread_state_count[NSTATES];
    Timestamp _thread_state_timestamp;
# endif
#endif

    // called by Master
    RouterThread(Master *master, int threadno);
    ~RouterThread();

    // task requests
    inline void add_pending();
#if HAVE_STRIDE_SCHED
    inline unsigned pass() const {
# if HAVE_TASK_HEAP
	return _task_heap.size() ? _task_heap.unchecked_at(0).pass : 0;
# else
	return _task_link._next->_pass;
# endif
    }
#endif

    // task running functions
    inline void driver_lock_tasks();
    inline void driver_unlock_tasks();
    inline void run_tasks(int ntasks);
    inline void process_pending();
    inline void run_os();
#if HAVE_ADAPTIVE_SCHEDULER
    void client_set_tickets(int client, int tickets);
    inline void client_update_pass(int client, const Timestamp &before);
#endif
#if HAVE_TASK_HEAP
    void task_reheapify_from(int pos, Task*);
#endif
    static inline bool running_in_interrupt();
    inline bool current_thread_is_running() const;
    void request_stop();
    inline void request_go();

    friend class Task;
    friend class Master;
#if CLICK_USERLEVEL
    friend class SelectSet;
#endif

};


/** @brief Returns this thread's ID.
 *
 * The result is >= 0 for true threads, and < 0 for threads that never run any
 * of their associated Tasks.
 */
inline int
RouterThread::thread_id() const
{
    return _id;
}

/** @brief Returns this thread's associated Master. */
inline Master*
RouterThread::master() const
{
    return _master;
}

/** @brief Returns whether any tasks are scheduled.
 *
 * Returns false iff no tasks are scheduled and no events are pending.  Since
 * not all events actually matter (for example, a Task might have been
 * scheduled and then subsequently unscheduled), active() may temporarily
 * return true even when no real events are outstanding.
 */
inline bool
RouterThread::active() const
{
#if HAVE_TASK_HEAP
    return _task_heap.size() != 0 || _pending_head;
#else
    return _task_link._next != &_task_link || _pending_head;
#endif
}

/** @brief Returns the beginning of the scheduled task list.
 *
 * Each RouterThread maintains a list of all currently-scheduled tasks.
 * Elements may traverse this list with the task_begin(), task_next(), and
 * task_end() functions, using iterator-like code such as:
 *
 * @code
 * thread->lock_tasks();
 * for (Task *t = thread->task_begin();
 *      t != thread->task_end();
 *      t = thread->task_next(t)) {
 *     // ... do something with t...
 * }
 * thread->unlock_tasks();
 * @endcode
 *
 * The thread's task lock must be held during the traversal, as shown above.
 *
 * The return value may not be a real task.  Test it against task_end() before
 * use.
 *
 * @sa task_next, task_end, lock_tasks, unlock_tasks
 */
inline Task *
RouterThread::task_begin() const
{
#if HAVE_TASK_HEAP
    return (_task_heap.size() ? _task_heap.unchecked_at(0).t : 0);
#else
    return static_cast<Task *>(_task_link._next);
#endif
}

/** @brief Returns the task following @a task in the scheduled task list.
 * @param task the current task
 *
 * The return value may not be a real task.  Test it against task_end() before
 * use.  However, the @a task argument must be a real task; do not attempt to
 * call task_next(task_end()).
 *
 * @sa task_begin for usage, task_end
 */
inline Task *
RouterThread::task_next(Task *task) const
{
#if HAVE_TASK_HEAP
    int p = task->_schedpos + 1;
    return (p < _task_heap.size() ? _task_heap.unchecked_at(p).t : 0);
#else
    return static_cast<Task *>(task->_next);
#endif
}

/** @brief Returns the end of the scheduled task list.
 *
 * The return value is not a real task.
 *
 * @sa task_begin for usage, task_next
 */
inline Task *
RouterThread::task_end() const
{
#if HAVE_TASK_HEAP
    return 0;
#else
    return static_cast<Task *>(const_cast<TaskLink *>(&_task_link));
#endif
}

inline bool
RouterThread::running_in_interrupt()
{
#if CLICK_LINUXMODULE
    return in_interrupt();
#else
    return false;
#endif
}

inline bool
RouterThread::current_thread_is_running() const
{
#if CLICK_LINUXMODULE
    return current == _linux_task && !running_in_interrupt();
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD && HAVE___THREAD_STORAGE_CLASS
    return click_current_thread_id == _id;
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD
    return click_current_processor() == _running_processor;
#else
    return true;
#endif
}

inline void
RouterThread::schedule_block_tasks()
{
    assert(!current_thread_is_running());
    ++_task_blocker_waiting;
}

inline void
RouterThread::block_tasks(bool scheduled)
{
    assert(!current_thread_is_running() && !running_in_interrupt());
    if (!scheduled)
	++_task_blocker_waiting;
    while (1) {
	uint32_t blocker = _task_blocker.value();
	if ((int32_t) blocker >= 0
	    && _task_blocker.compare_swap(blocker, blocker + 1) == blocker)
	    break;
#if CLICK_LINUXMODULE
	// 3.Nov.2008: Must allow other threads a chance to run.  Otherwise,
	// soft lock is possible: the thread in block_tasks() waits for
	// RouterThread::_linux_task to complete a task set, but
	// RouterThread::_linux_task can't run until the thread in
	// block_tasks() relinquishes the CPU.
	//
	// We might be able to avoid schedule() in some cases, but don't
	// bother to try.
	schedule();
#endif
    }
    --_task_blocker_waiting;
}

inline void
RouterThread::unblock_tasks()
{
    assert((int32_t) _task_blocker.value() > 0);
    --_task_blocker;
}

inline void
RouterThread::lock_tasks()
{
    assert(!running_in_interrupt());
    if (unlikely(!current_thread_is_running())) {
	block_tasks(false);
	_task_lock.acquire();
    }
}

inline void
RouterThread::unlock_tasks()
{
    assert(!running_in_interrupt());
    if (unlikely(!current_thread_is_running())) {
	_task_lock.release();
	unblock_tasks();
    }
}

inline void
RouterThread::wake()
{
#if CLICK_LINUXMODULE
    struct task_struct *task = _linux_task;
    if (task)
	wake_up_process(task);
#elif CLICK_USERLEVEL
    // see also Master::add_select()
    if (!current_thread_is_running())
	_selects.wake_immediate();
#elif CLICK_BSDMODULE && !BSD_NETISRSCHED
    if (_sleep_ident)
	wakeup_one(&_sleep_ident);
#endif
}

inline void
RouterThread::add_pending()
{
    wake();
}

inline bool
RouterThread::stop_flag() const
{
    return _stop_flag;
}

inline void
RouterThread::request_go()
{
    _stop_flag = 0;
}

inline void
RouterThread::set_thread_state(int state)
{
    (void) state;
#if CLICK_DEBUG_SCHEDULING
    assert(state >= 0 && state < NSTATES);
# if CLICK_DEBUG_SCHEDULING > 1
    Timestamp now = Timestamp::now();
    if (_thread_state_timestamp)
	_thread_state_time[_thread_state] += now - _thread_state_timestamp;
    if (_thread_state != state)
	++_thread_state_count[_thread_state];
    _thread_state_timestamp = now;
# endif
    _thread_state = state;
#endif
}

inline void
RouterThread::set_thread_state_for_blocking(int delay_type)
{
    if (delay_type < 0)
	set_thread_state(S_BLOCKED);
    else
	set_thread_state(delay_type ? S_TIMERWAIT : S_PAUSED);
}

#if CLICK_DEBUG_SCHEDULING > 1
inline Timestamp
RouterThread::thread_state_time(int state) const
{
    assert(state >= 0 && state < NSTATES);
    return _thread_state_time[state];
}

inline uint64_t
RouterThread::thread_state_count(int state) const
{
    assert(state >= 0 && state < NSTATES);
    return _thread_state_count[state];
}
#endif

CLICK_ENDDECLS
#endif
