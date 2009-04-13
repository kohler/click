// -*- c-basic-offset: 4; related-file-name: "../../lib/master.cc" -*-
#ifndef CLICK_MASTER_HH
#define CLICK_MASTER_HH
#include <click/router.hh>
#include <click/atomic.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
# include <signal.h>
# if HAVE_POLL_H
#  include <poll.h>
# endif
#endif
#if CLICK_NS
# include <click/simclick.h>
#endif
CLICK_DECLS
class Element;

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
    void run_timers();
    unsigned max_timer_stride() const		{ return _max_timer_stride; }
    unsigned timer_stride() const		{ return _timer_stride; }
    void set_max_timer_stride(unsigned timer_stride);

#if CLICK_USERLEVEL
    int add_select(int fd, Element*, int mask);
    int remove_select(int fd, Element*, int mask);
    void run_selects(bool more_tasks);

    int add_signal_handler(int signo, Router*, const String &handler);
    int remove_signal_handler(int signo, Router*, const String &handler);
    inline void run_signals();
#endif

    void kill_router(Router*);

#if CLICK_NS
    void initialize_ns(simclick_node_t *simnode);
    simclick_node_t *simnode() const		{ return _simnode; }
#endif

#if CLICK_DEBUG_MASTER
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
    void lock_timers();
    bool attempt_lock_timers();
    void unlock_timers();
    void timer_reheapify_from(int, Timer*, bool will_delete);
    inline void run_one_timer(Timer *);

#if CLICK_USERLEVEL
    // SELECT
# if HAVE_SYS_EVENT_H && HAVE_KQUEUE
    int _kqueue;
    int _selected_callno;
    Vector<int> _selected_callnos;
# endif
# if !HAVE_POLL_H
    struct pollfd {
	int fd;
	int events;
    };
    fd_set _read_select_fd_set;
    fd_set _write_select_fd_set;
    int _max_select_fd;
# endif /* HAVE_POLL_H */
    Vector<struct pollfd> _pollfds;
    Vector<Element*> _read_poll_elements;
    Vector<Element*> _write_poll_elements;
    Spinlock _select_lock;
    void remove_pollfd(int pi, int event);
# if HAVE_SYS_EVENT_H && HAVE_KQUEUE
    void run_selects_kqueue(bool);
# endif
# if HAVE_POLL_H
    void run_selects_poll(bool);
# else
    void run_selects_select(bool);
# endif

    // SIGNALS
    struct SignalInfo {
	int signo;
	Router *router;
	String handler;
	SignalInfo *next;
    };
    SignalInfo *_siginfo;
    bool _signal_adding;
    Spinlock _signal_lock;
    void process_signals();
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
Master::run_signals()
{
    if (signals_pending)
	process_signals();
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
    if (_timer_stride >= 8 || e.sec() == 0)
	/* do nothing */;
    else if (_timer_stride >= 4)
	e -= Timer::adjustment();
    else
	e -= Timer::adjustment() + Timer::adjustment();
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
