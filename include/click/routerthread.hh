// -*- c-basic-offset: 4; related-file-name: "../../lib/routerthread.cc" -*-
#ifndef CLICK_ROUTERTHREAD_HH
#define CLICK_ROUTERTHREAD_HH
#include <click/sync.hh>
#include <click/vector.hh>
#include <click/ewma.hh>
// NB: user must #include <click/task.hh> before <click/routerthread.hh>.
// We cannot #include <click/task.hh> ourselves because of circular #include
// dependency.
CLICK_DECLS

class RouterThread : public Task { public:

    RouterThread(Router *);
    ~RouterThread();
  
    Router *router() const		{ return _router; }
    int thread_id() const		{ return _id; }

    void driver();
    void driver_once();

    // Task list functions
    bool empty() const;

    void lock_tasks();
    bool attempt_lock_tasks();
    void unlock_tasks();

    void unschedule_all_tasks();

#ifdef HAVE_ADAPTIVE_SCHEDULER
    // min_cpu_share() and max_cpu_share() are expressed on ab scale with
    // Task::MAX_UTILIZATION == 100%.
    unsigned min_cpu_share() const	{ return _min_click_share; }
    unsigned max_cpu_share() const	{ return _max_click_share; }
    unsigned cur_cpu_share() const	{ return _cur_click_share; }
    void set_cpu_share(unsigned min_share, unsigned max_share);
#endif

  private:
    
    Router *_router;
    int _id;

    Spinlock _lock;
    uatomic32_t _task_lock_waiting;
    uatomic32_t _pending;

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

    // called by Router
    void set_thread_id(int i)		{ _id = i; }

    // task request IDs
    void add_pending()			{ _pending++; }
    void process_task_requests();

    // task running functions
    inline void nice_lock_tasks();
    inline void run_tasks(int ntasks);
    inline void run_os();
#ifdef HAVE_ADAPTIVE_SCHEDULER
    void client_set_tickets(int client, int tickets);
    inline void client_update_pass(int client, const struct timeval &before, const struct timeval &after);
    inline void check_restride(struct timeval &before, const struct timeval &now, int &restride_iter);
#endif
    void wait(int iter);
    
    friend class Task;
    friend class Router;

};


inline bool
RouterThread::empty() const
{
    return (const Task *)_next == this;
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

CLICK_ENDDECLS
#endif
