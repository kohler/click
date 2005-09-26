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

#ifdef CLICK_BSDMODULE
# include <machine/cpu.h>
# include <assert.h>	/* MARKO XXX */
#endif

#if CLICK_BSDMODULE && !BSD_NETISRSCHED
# define SPLCHECK				\
	int s = splimp();			\
	if (s == 0)				\
	    panic("not spl'ed: %d\n", s);	\
	splx(s)
#else
# define SPLCHECK
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

    inline bool initialized() const;
    inline bool scheduled() const;

    inline TaskHook hook() const;
    inline void *thunk() const;
    inline Element *element() const;

    Master *master() const;
    inline RouterThread *thread() const;

    inline int home_thread_id() const;
    void set_home_thread_id(int);
 
#ifdef HAVE_STRIDE_SCHED
    inline int tickets() const;
    inline void set_tickets(int);
    inline void adjust_tickets(int);
#endif

    void initialize(Router *, bool scheduled);
    void initialize(Element *, bool scheduled);

    void unschedule();
    inline void reschedule();

    inline void fast_unschedule();
#ifdef HAVE_TASK_HEAP
    void fast_reschedule();
#else
    inline void fast_reschedule();
#endif

    void strong_unschedule();
    void strong_reschedule();

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

    enum { RESCHEDULE = 1, CHANGE_THREAD = 2 };
    unsigned _pending;
    Task* _pending_next;

    Task(const Task&);
    Task& operator=(const Task&);
    void cleanup();
    
    void add_pending(int);
    void process_pending(RouterThread*);
    inline void fast_schedule();
    void true_reschedule();
    inline void lock_tasks();
    inline bool attempt_lock_tasks();

    void make_list();
    static bool error_hook(Task*, void*);
  
    friend class RouterThread;
    friend class Master;
  
};


// need RouterThread's definition for inline functions
CLICK_ENDDECLS
#include <click/routerthread.hh>
CLICK_DECLS


/** @brief Construct a task that calls @a hook with @a thunk argument.
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
      _router(0), _pending(0), _pending_next(0)
{
}

/** @brief Construct a task that calls @a e ->@link Element::run_task()
 * run_task().@endlink
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
      _router(0), _pending(0), _pending_next(0)
{
}

inline bool
Task::initialized() const
{
    return _router != 0;
}

inline bool
Task::scheduled() const
{
#if HAVE_TASK_HEAP
    return _schedpos >= 0;
#else
    return _prev != 0;
#endif
}

inline TaskHook
Task::hook() const
{
    return _hook;
}

inline void *
Task::thunk() const
{
    return _thunk;
}

inline Element *
Task::element()	const
{ 
    return _hook ? 0 : reinterpret_cast<Element*>(_thunk); 
}

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
    // assert(!intr_nesting_level);
    SPLCHECK;
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

inline int
Task::tickets() const
{
    return _tickets;
}

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

inline void 
Task::adjust_tickets(int delta)
{
    set_tickets(_tickets + delta);
}

#ifndef HAVE_TASK_HEAP
inline void
Task::fast_reschedule()
{
    // should not be scheduled at this point
    assert(_thread);
#if CLICK_LINUXMODULE
    // tasks never run at interrupt time in Linux
    assert(!in_interrupt());
#endif
#if CLICK_BSDMODULE
    // assert(!intr_nesting_level); it happens all the time from fromdevice!
    SPLCHECK;
#endif

    if (!scheduled()) {
	// increase pass
	_pass += _stride;

#if 0
	// look for 'n' immediately before where we should be scheduled
	Task* n = _thread->_prev;
	while (n != _thread && PASS_GT(n->_pass, _pass))
	    n = n->_prev;

	// schedule after 'n'
	_next = n->_next;
	_prev = n;
	n->_next = this;
	_next->_prev = this;
#else
	// look for 'n' immediately after where we should be scheduled
	Task* n = _thread->_next;
#ifdef CLICK_BSDMODULE /* XXX MARKO a race occured here when not spl'ed */
	while (n->_next != NULL && n != _thread && !PASS_GT(n->_pass, _pass))
#else
	while (n != _thread && !PASS_GT(n->_pass, _pass))
#endif
	    n = n->_next;
    
	// schedule before 'n'
	_prev = n->_prev;
	_next = n;
	_prev->_next = this;
	n->_prev = this;
#endif
    }
}
#endif

inline void
Task::fast_schedule()
{
    SPLCHECK;
    assert(_tickets >= 1);
#if HAVE_TASK_HEAP
    _pass = (_thread->empty() ? 0 : _thread->_task_heap[0]->_pass);
#else
    _pass = _thread->_next->_pass;
#endif
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
    SPLCHECK;
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

inline void
Task::reschedule()
{
    SPLCHECK;
    assert(_thread);
    if (!scheduled())
	true_reschedule();
}


inline void
Task::call_hook()
{
#if __MTCLICK__
    _cycle_runs++;
#endif
#ifdef HAVE_ADAPTIVE_SCHEDULER
    _runs++;
    if (!_hook)
	_work_done += ((Element*)_thunk)->run_task();
    else
	_work_done += _hook(this, _thunk);
#else
    if (!_hook)
	(void) ((Element*)_thunk)->run_task();
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

inline int
Task::home_thread_id() const
{
    return _home_thread_id;
}

CLICK_ENDDECLS
#endif
