// -*- c-basic-offset: 4; related-file-name: "../../lib/master.cc" -*-
#ifndef CLICK_MASTER_HH
#define CLICK_MASTER_HH
#include <click/router.hh>
#include <click/atomic.hh>
#include <click/timerset.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
# include <signal.h>
# if !HAVE_ALLOW_SELECT && !HAVE_ALLOW_POLL && !HAVE_ALLOW_KQUEUE
#  define HAVE_ALLOW_SELECT 1
# endif
# if defined(__APPLE__) && HAVE_ALLOW_SELECT && HAVE_ALLOW_POLL
// Apple's poll() is often broken
#  undef HAVE_ALLOW_POLL
# endif
# if HAVE_POLL_H && HAVE_ALLOW_POLL
#  include <poll.h>
# else
#  undef HAVE_ALLOW_POLL
#  if !HAVE_ALLOW_SELECT && !HAVE_ALLOW_KQUEUE
#   error "poll is not supported on this system, try --enable-select"
#  endif
# endif
# if !HAVE_SYS_EVENT_H || !HAVE_KQUEUE
#  undef HAVE_ALLOW_KQUEUE
#  if !HAVE_ALLOW_SELECT && !HAVE_ALLOW_POLL
#   error "kqueue is not supported on this system, try --enable-select"
#  endif
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
    bool paused() const				{ return _master_paused > 0; }

    inline int nthreads() const;
    inline RouterThread* thread(int id) const;
    void wake_somebody();

    const volatile int* stopper_ptr() const	{ return &_stopper; }

    TimerSet &timer_set()			{ return _ts; }
    const TimerSet &timer_set() const		{ return _ts; }

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

    // TIMER
    TimerSet _ts;

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
# if HAVE_ALLOW_KQUEUE
    int _kqueue;
# endif
# if !HAVE_ALLOW_POLL
    struct pollfd {
	int fd;
	int events;
    };
    fd_set _read_select_fd_set;
    fd_set _write_select_fd_set;
    int _max_select_fd;
# endif /* !HAVE_ALLOW_POLL */
    Vector<struct pollfd> _pollfds;
    Vector<ElementSelector> _element_selectors;
    Vector<int> _fd_to_pollfd;
    Spinlock _select_lock;
    void register_select(int fd, bool add_read, bool add_write);
    void remove_pollfd(int pi, int event);
    inline void call_selected(int fd, int mask) const;
# if HAVE_ALLOW_KQUEUE
    void run_selects_kqueue(RouterThread *thread, bool more_tasks);
# endif
# if HAVE_ALLOW_POLL
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
    friend class RouterThread;
    friend class Router;

};

inline int
Master::nthreads() const
{
    return _threads.size() - 1;
}

inline RouterThread*
Master::thread(int id) const
{
    // return the requested thread, or the quiescent thread if there's no such
    // thread
    if ((unsigned)(id + 1) < (unsigned)_threads.size())
	return _threads.at_u(id + 1);
    else
	return _threads.at_u(0);
}

inline void
Master::wake_somebody()
{
    _threads.at_u(1)->wake();
}

inline TimerSet &
RouterThread::timer_set()
{
    return _master->timer_set();
}

inline const TimerSet &
RouterThread::timer_set() const
{
    return _master->timer_set();
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
    t = next_timer_expiry_adjusted();
    if (t.sec() == 0)
	return -1;		// block forever
    else if (unlikely(Timestamp::warp_jumping())) {
	Timestamp::warp_jump(t);
	return 0;
    } else if ((t -= Timestamp::now(), t.sec() >= 0)) {
	t = t.warp_real_delay();
	return 1;
    } else
	return 0;
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

inline Master *
Element::master() const
{
    return _router->master();
}

CLICK_ENDDECLS
#endif
