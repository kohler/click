// -*- c-basic-offset: 4; related-file-name: "../../lib/master.cc" -*-
#ifndef CLICK_MASTER_HH
#define CLICK_MASTER_HH
#include <click/router.hh>
#include <click/atomic.hh>
#if CLICK_USERLEVEL
# include <signal.h>
#endif
#if CLICK_NS
# include <click/simclick.h>
#endif
CLICK_DECLS

#define CLICK_DEBUG_MASTER 0

class Master { public:

    Master(int nthreads);
    ~Master();

    void use();
    void unuse();

    void block_all();
    void unblock_all();

    void pause();
    inline void unpause();
    bool paused() const				{ return _master_paused > 0; }

    inline int nthreads() const;
    inline RouterThread *thread(int id) const;
    inline RouterThread *unchecked_thread(int id) const;
    void wake_somebody();

#if CLICK_USERLEVEL
    int add_signal_handler(int signo, Router *router, String handler);
    int remove_signal_handler(int signo, Router *router, String handler);
    void process_signals(RouterThread *thread);
    static void signal_handler(int signo);	// not really public
#endif

    Router *root_router() const			{ return _root_router; }
    void kill_router(Router *router);

#if CLICK_NS
    void initialize_ns(simclick_node_t *simnode);
    simclick_node_t *simnode() const		{ return _simnode; }
#endif

#if CLICK_DEBUG_MASTER || CLICK_DEBUG_SCHEDULING
    String info() const;
#endif

#if CLICK_USERLEVEL
    static volatile sig_atomic_t signals_pending;
#endif

  private:

    // THREADS
    struct aligned_thread {
	RouterThread t;
	char padding[CLICK_CACHE_LINE_PAD_BYTES(sizeof(RouterThread))];
    };
    aligned_thread *_threads;
    int _nthreads;
    size_t _threads_byte_offset;

    // ROUTERS
    Router *_routers;
    int _refcount;
    Router *_root_router;
    void register_router(Router *r);
    void prepare_router(Router *r);
    void run_router(Router *r, bool foreground);
    void unregister_router(Router *r);

    // LOCKS
    atomic_uint32_t _master_paused CLICK_ALIGNED(CLICK_CACHE_LINE_SIZE);
#if CLICK_LINUXMODULE
    spinlock_t _master_lock;
    struct task_struct *_master_lock_task;
    int _master_lock_count;
#elif HAVE_MULTITHREAD
    Spinlock _master_lock;
#endif
    inline void lock_master();
    inline void unlock_master();

    // DRIVERMANAGER
    inline void request_stop();
    inline void request_go();
    bool check_driver();

#if CLICK_USERLEVEL
    // SIGNALS
    struct SignalInfo {
	int signo;
	Router *router;
	String handler;
	SignalInfo *next;
	SignalInfo(int signo_, Router *router_, const String &handler_)
	    : signo(signo_), router(router_), handler(handler_), next() {
	}
	bool equals(int signo_, Router *router_, const String &handler_) const {
	    return signo == signo_ && router == router_ && handler == handler_;
	}
    };
    SignalInfo *_siginfo;
    sigset_t _sig_dispatching;
    Spinlock _signal_lock;
#endif

#if CLICK_NS
    simclick_node_t *_simnode;
#endif

    Master(const Master&);
    Master& operator=(const Master&);

    friend class Task;
    friend class RouterThread;
    friend class Router;

};

inline int
Master::nthreads() const
{
    return _nthreads;
}

inline RouterThread *
Master::unchecked_thread(int id) const
{
    return &_threads[id + 1].t;
}

inline RouterThread *
Master::thread(int id) const
{
    // return the requested thread, or the quiescent thread if there's no such
    // thread
    if (unsigned(id + 1) <= unsigned(_nthreads))
	return &_threads[id + 1].t;
    else
	return &_threads[0].t;
}

inline void
Master::wake_somebody()
{
    unchecked_thread(0)->wake();
}

#if CLICK_USERLEVEL
inline void
RouterThread::run_signals()
{
    if (Master::signals_pending)
	_master->process_signals(this);
}

inline int
TimerSet::next_timer_delay(bool more_tasks, Timestamp &t) const
{
# if CLICK_NS
    // The simulator should never block.
    (void) more_tasks, (void) t;
    return 0;
# else
    if (more_tasks || Master::signals_pending)
	return 0;
    t = timer_expiry_steady_adjusted();
    if (!t)
	return -1;		// block forever
    else if (unlikely(Timestamp::warp_jumping())) {
	Timestamp::warp_jump_steady(t);
	return 0;
    } else if ((t -= Timestamp::now_steady(), !t.is_negative())) {
	t = t.warp_real_delay();
	return 1;
    } else
	return 0;
# endif
}
#endif

inline void
Master::request_stop()
{
    for (aligned_thread *tp = _threads; tp != _threads + _nthreads + 1; ++tp)
	tp->t.request_stop();
    // ensure that at least one thread is awake to handle the stop event
    wake_somebody();
}

inline void
Master::request_go()
{
    for (aligned_thread *tp = _threads; tp != _threads + _nthreads + 1; ++tp)
	tp->t.request_go();
}

inline void
Master::lock_master()
{
#if CLICK_LINUXMODULE
    if (current != _master_lock_task) {
	spin_lock(&_master_lock);
	_master_lock_task = current;
    } else
	_master_lock_count++;
#elif HAVE_MULTITHREAD
    _master_lock.acquire();
#endif
}

inline void
Master::unlock_master()
{
#if CLICK_LINUXMODULE
    assert(current == _master_lock_task);
    if (_master_lock_count == 0) {
	_master_lock_task = 0;
	spin_unlock(&_master_lock);
    } else
	_master_lock_count--;
#elif HAVE_MULTITHREAD
    _master_lock.release();
#endif
}

inline void
Master::unpause()
{
    _master_paused--;
}

inline Master *
Element::master() const
{
    return _router->master();
}

CLICK_ENDDECLS
#endif
