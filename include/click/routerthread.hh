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

// NB: user must #include <click/task.hh> before <click/routerthread.hh>.
// We cannot #include <click/task.hh> ourselves because of circular #include
// dependency.
CLICK_DECLS

class RouterThread : public Task { public:

    int thread_id() const		{ return _id; }
    Master *master() const		{ return _master; }

    void driver();
    void driver_once();

    // Task list functions
    bool empty() const;

    void lock_tasks();
    bool attempt_lock_tasks();
    void unlock_tasks();

    void unschedule_all_tasks();

#ifdef HAVE_ADAPTIVE_SCHEDULER
    // min_cpu_share() and max_cpu_share() are expressed on a scale with
    // Task::MAX_UTILIZATION == 100%.
    unsigned min_cpu_share() const	{ return _min_click_share; }
    unsigned max_cpu_share() const	{ return _max_click_share; }
    unsigned cur_cpu_share() const	{ return _cur_click_share; }
    void set_cpu_share(unsigned min_share, unsigned max_share);
#endif

    inline void unsleep();



    unsigned _tasks_per_iter;
    unsigned _iters_per_timers;
    unsigned _iters_per_os;


  private:
    
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
    Task *_wakeup_list;
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
    
    friend class Task;
    friend class Master;

};


inline bool
RouterThread::empty() const
{
    return ((const Task *)_next == this) && !_pending;
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
}

inline void
RouterThread::add_pending()
{
    _pending++;
    unsleep();
}

CLICK_ENDDECLS
#endif
