// -*- c-basic-offset: 4; related-file-name: "../../lib/master.cc" -*-
#ifndef CLICK_MASTER_HH
#define CLICK_MASTER_HH
#include <click/router.hh>
#include <click/atomic.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
# include <signal.h>
# if HAVE_POLL_H && !HAVE_USE_SELECT
#  include <poll.h>
# endif
# if HAVE_SYS_EVENT_H && HAVE_KQUEUE && !HAVE_USE_SELECT && !HAVE_USE_POLL && !defined(HAVE_USE_KQUEUE)
#  define HAVE_USE_KQUEUE 1
# elif (!HAVE_SYS_EVENT_H || !HAVE_KQUEUE) && HAVE_USE_KQUEUE
#  error "--enable-select=kqueue is not supported on this system"
# endif
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

    void pause();
    inline void unpause();

    inline int nthreads() const;
    inline RouterThread* thread(int id) const;

    const volatile int* stopper_ptr() const	{ return &_stopper; }

    Timestamp next_timer_expiry() const		{ return _timer_expiry; }
    const Timestamp &timer_check() const	{ return _timer_check; }
    void run_timers(RouterThread *thread);
    unsigned max_timer_stride() const		{ return _max_timer_stride; }
    unsigned timer_stride() const		{ return _timer_stride; }
    void set_max_timer_stride(unsigned timer_stride);

#if CLICK_USERLEVEL
    int add_select(int fd, Element *element, int mask);
    int remove_select(int fd, Element *element, int mask);
    void run_selects(RouterThread *thread);

    int add_signal_handler(int signo, Router *router, String handler);
    int remove_signal_handler(int signo, Router *router, String handler);
#endif

    void kill_router(Router*);

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

    // stick _timer_expiry here so it will most likely fit in a cache line,
    // & we don't have to worry about its parts being updated separately
    Timestamp _timer_expiry;

#if CLICK_LINUXMODULE
    spinlock_t _master_lock;
    struct task_struct *_master_lock_task;
    int _master_lock_count;
#elif HAVE_MULTITHREAD
    Spinlock _master_lock;
#endif
    atomic_uint32_t _master_paused;
    inline void lock_master();
    inline void unlock_master();

    // ROUTERS
    Router* _routers;
    int _refcount;
    void register_router(Router*);
    void prepare_router(Router*);
    void run_router(Router*, bool foreground);
    void unregister_router(Router*);

    // THREADS
    Vector<RouterThread*> _threads;

    // DRIVERMANAGER
    volatile int _stopper;
    inline void set_stopper(int);
    bool check_driver();

    // PENDING TASKS
    uintptr_t _pending_head;
    volatile uintptr_t *_pending_tail;
    SpinlockIRQ _master_task_lock;
    void process_pending(RouterThread*);

    // TIMERS
    unsigned _max_timer_stride;
    unsigned _timer_stride;
    unsigned _timer_count;
    Vector<Timer *> _timer_heap;
    Vector<Timer *> _timer_runchunk;
#if CLICK_LINUXMODULE
    spinlock_t _timer_lock;
    struct task_struct *_timer_task;
#elif HAVE_MULTITHREAD
    Spinlock _timer_lock;
#endif
    Timestamp _timer_check;
    uint32_t _timer_check_reports;
    inline Timestamp next_timer_expiry_adjusted() const;
#if CLICK_USERLEVEL
    inline int next_timer_delay(bool more_tasks, Timestamp &t) const;
#endif
    void lock_timers();
    bool attempt_lock_timers();
    void unlock_timers();
    inline void run_one_timer(Timer *);

    void set_timer_expiry() {
	if (_timer_heap.size())
	    _timer_expiry = _timer_heap.at_u(0)->_expiry;
	else
	    _timer_expiry = Timestamp();
    }
    void check_timer_expiry(Timer *t);
    static inline void place_timer(Timer **t, Timer **tbegin) {
	(*t)->_schedpos1 = (t - tbegin) + 1;
    }

    struct timer_less {
	bool operator()(Timer *a, Timer *b) {
	    return a->expiry() < b->expiry();
	}
    };
    struct timer_place {
	Timer **_begin;
	timer_place(Timer **begin)
	    : _begin(begin) {
	}
	void operator()(Timer **t) {
	    Master::place_timer(t, _begin);
	}
    };

#if CLICK_USERLEVEL
    // SELECT
    struct ElementSelector {
	Element *read;
	Element *write;
	ElementSelector()
	    : read(0), write(0)
	{
	}
    };
# if HAVE_USE_KQUEUE
    int _kqueue;
# endif
# if !HAVE_POLL_H || HAVE_USE_SELECT
    struct pollfd {
	int fd;
	int events;
    };
    fd_set _read_select_fd_set;
    fd_set _write_select_fd_set;
    int _max_select_fd;
# endif /* !HAVE_POLL_H || HAVE_USE_SELECT */
    Vector<struct pollfd> _pollfds;
    Vector<ElementSelector> _element_selectors;
    Vector<int> _fd_to_pollfd;
    Spinlock _select_lock;
    void register_select(int fd, bool add_read, bool add_write);
    void remove_pollfd(int pi, int event);
    inline void call_selected(int fd, int mask) const;
# if HAVE_USE_KQUEUE
    void run_selects_kqueue(RouterThread *thread, bool more_tasks);
# endif
# if HAVE_POLL_H && !HAVE_USE_SELECT
    void run_selects_poll(RouterThread *thread, bool more_tasks);
# else
    void run_selects_select(RouterThread *thread, bool more_tasks);
# endif

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
    void process_signals(RouterThread *thread);
    inline void run_signals(RouterThread *thread);
#endif

#if CLICK_NS
    simclick_node_t *_simnode;
#endif

    Master(const Master&);
    Master& operator=(const Master&);

    friend class Task;
    friend class Timer;
    friend class RouterThread;
    friend class Router;

};

inline int
Master::nthreads() const
{
    return _threads.size() - 2;
}

inline RouterThread*
Master::thread(int id) const
{
    // return the requested thread, or the quiescent thread if there's no such
    // thread
    if ((unsigned)(id + 2) < (unsigned)_threads.size())
	return _threads.at_u(id + 2);
    else
	return _threads.at_u(1);
}

#if CLICK_USERLEVEL
inline void
Master::run_signals(RouterThread *thread)
{
# if HAVE_MULTITHREAD
    if (thread->_wake_pipe_pending || signals_pending)
	process_signals(thread);
# else
    if (signals_pending)
	process_signals(thread);
# endif
}
#endif

inline void
Master::set_stopper(int s)
{
    _stopper = s;
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

inline Timestamp
Master::next_timer_expiry_adjusted() const
{
    Timestamp e = _timer_expiry;
#if CLICK_USERLEVEL
    if (likely(!Timestamp::warp_jumping())) {
#endif
    if (_timer_stride >= 8 || e.sec() == 0)
	/* do nothing */;
    else if (_timer_stride >= 4)
	e -= Timer::adjustment();
    else
	e -= Timer::adjustment() + Timer::adjustment();
#if CLICK_USERLEVEL
    }
#endif
    return e;
}

inline void
Master::lock_timers()
{
#if CLICK_LINUXMODULE
    if (current != _timer_task)
	spin_lock(&_timer_lock);
#elif HAVE_MULTITHREAD
    _timer_lock.acquire();
#endif
}

inline bool
Master::attempt_lock_timers()
{
#if CLICK_LINUXMODULE
    return spin_trylock(&_timer_lock);
#elif HAVE_MULTITHREAD
    return _timer_lock.attempt();
#else
    return true;
#endif
}

inline void
Master::unlock_timers()
{
#if CLICK_LINUXMODULE
    if (current != _timer_task)
	spin_unlock(&_timer_lock);
#elif HAVE_MULTITHREAD
    _timer_lock.release();
#endif
}

inline Master *
Element::master() const
{
    return _router->master();
}

CLICK_ENDDECLS
#endif
