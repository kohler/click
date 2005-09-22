// -*- c-basic-offset: 4; related-file-name: "../../lib/routerthread.cc" -*-
#ifndef CLICK_ROUTERTHREAD_HH
#define CLICK_ROUTERTHREAD_HH
#include <click/sync.hh>
#include <click/vector.hh>
#include <click/ewma.hh>
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/sched.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
#if CLICK_BSDMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <sys/systm.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif

#define CLICK_DEBUG_SCHEDULING 0

// NB: user must #include <click/task.hh> before <click/routerthread.hh>.
// We cannot #include <click/task.hh> ourselves because of circular #include
// dependency.
CLICK_DECLS

class RouterThread
#ifndef HAVE_TASK_HEAP
    : public Task
#endif
{ public:

    inline int thread_id() const;
    inline Master* master() const;

    void driver();
    void driver_once();

    // Task list functions
    inline bool empty() const;
    inline Task *task_begin() const;
    inline Task *task_end() const;
    inline Task *task_next(Task *) const;
    
    inline void lock_tasks();
    inline bool attempt_lock_tasks();
    inline void unlock_tasks();

    void unschedule_router_tasks(Router*);

#ifdef HAVE_ADAPTIVE_SCHEDULER
    // min_cpu_share() and max_cpu_share() are expressed on a scale with
    // Task::MAX_UTILIZATION == 100%.
    unsigned min_cpu_share() const	{ return _min_click_share; }
    unsigned max_cpu_share() const	{ return _max_click_share; }
    unsigned cur_cpu_share() const	{ return _cur_click_share; }
    void set_cpu_share(unsigned min_share, unsigned max_share);
#endif

    inline void unsleep();

#if CLICK_DEBUG_SCHEDULING
    enum { S_RUNNING, S_PAUSED, S_TIMER, S_BLOCKED };
    int thread_state() const		{ return _thread_state; }
    static String thread_state_name(int);
    uint32_t driver_epoch() const	{ return _driver_epoch; }
    uint32_t driver_task_epoch() const	{ return _driver_task_epoch; }
    timeval task_epoch_time(uint32_t epoch) const;
# ifdef CLICK_LINUXMODULE
    struct task_struct *sleeper() const	{ return _sleeper; }
# endif
#endif

    unsigned _tasks_per_iter;
    unsigned _iters_per_timers;
    unsigned _iters_per_os;

  private:

#ifdef HAVE_TASK_HEAP
    Vector<Task*> _task_heap;
    int _task_heap_hole;
#endif
    
    Master *_master;
    int _id;

    Spinlock _lock;
    atomic_uint32_t _task_lock_waiting;
    atomic_uint32_t _pending;

#ifdef CLICK_LINUXMODULE
    struct task_struct *_sleeper;
#endif
    
#ifdef CLICK_BSDMODULE
    // XXX FreeBSD
    u_int64_t _old_tsc; /* MARKO - temp. */
    void *_sleep_ident;
    int _oticks;
#endif

#ifdef HAVE_ADAPTIVE_SCHEDULER
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
#endif

#if CLICK_DEBUG_SCHEDULING
    int _thread_state;
    uint32_t _driver_epoch;
    uint32_t _driver_task_epoch;
    enum { TASK_EPOCH_BUFSIZ = 32 };
    uint32_t _task_epoch_first;
    timeval _task_epoch_time[TASK_EPOCH_BUFSIZ];
#endif
    
    // called by Master
    RouterThread(Master *, int);
    ~RouterThread();

    // task requests
    inline void add_pending();

    // task running functions
    inline void nice_lock_tasks();
    inline void run_tasks(int ntasks);
    inline void run_os();
#ifdef HAVE_ADAPTIVE_SCHEDULER
    void client_set_tickets(int client, int tickets);
    inline void client_update_pass(int client, const struct timeval &before, const struct timeval &after);
    inline void check_restride(struct timeval &before, const struct timeval &now, int &restride_iter);
#endif
#ifdef HAVE_TASK_HEAP
    void task_reheapify_from(int pos, Task*);
#endif
    
    friend class Task;
    friend class Master;

};


inline int
RouterThread::thread_id() const
{
    return _id;
}

inline Master*
RouterThread::master() const
{
    return _master;
}

inline bool
RouterThread::empty() const
{
#ifdef HAVE_TASK_HEAP
    return _task_heap.size() == 0 && !_pending;
#else
    return ((const Task *)_next == this) && !_pending;
#endif
}

inline Task *
RouterThread::task_begin() const
{
#ifdef HAVE_TASK_HEAP
    int p = _task_heap_hole;
    return (p < _task_heap.size() ? _task_heap[p] : 0);
#else
    return _next;
#endif
}

inline Task *
RouterThread::task_end() const
{
#ifdef HAVE_TASK_HEAP
    return 0;
#else
    return (Task *) this;
#endif
}

inline Task *
RouterThread::task_next(Task *t) const
{
#ifdef HAVE_TASK_HEAP
    int p = t->_schedpos + 1;
    return (p < _task_heap.size() ? _task_heap[p] : 0);
#else
    return t->_next;
#endif
}

inline void
RouterThread::lock_tasks()
{
    _task_lock_waiting++;
    _lock.acquire();
    _task_lock_waiting--;
}

inline bool
RouterThread::attempt_lock_tasks()
{
    return _lock.attempt();
}

inline void
RouterThread::unlock_tasks()
{
    _lock.release();
}

inline void
RouterThread::unsleep()
{
#ifdef CLICK_LINUXMODULE
    if (_sleeper)
	wake_up_process(_sleeper);
#endif
#if CLICK_BSDMODULE && !BSD_NETISRSCHED
    if (_sleep_ident)
	wakeup_one(&_sleep_ident);
#endif
}

inline void
RouterThread::add_pending()
{
    _pending++;
    unsleep();
}

CLICK_ENDDECLS
#endif
