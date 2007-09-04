// -*- c-basic-offset: 4; related-file-name: "../../lib/master.cc" -*-
#ifndef CLICK_MASTER_HH
#define CLICK_MASTER_HH
#include <click/vector.hh>
#include <click/timer.hh>
#include <click/task.hh>
#include <click/sync.hh>
#include <click/atomic.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
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
    void run_timers();
    
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
    void initialize_ns(simclick_sim, simclick_click);
    simclick_sim siminst() const		{ return _siminst; }
    simclick_click clickinst() const		{ return _clickinst; }
#endif
    
#if CLICK_DEBUG_MASTER
    String info() const;
#endif

#if CLICK_USERLEVEL
    static atomic_uint32_t signals_pending;
#endif
    
  private:

    // stick _timer_expiry here so it will most likely fit in a cache line,
    // & we don't have to worry about its parts being updated separately
    Timestamp _timer_expiry;
    
#if CLICK_LINUXMODULE
    spinlock_t _master_lock;
    struct task_struct *_master_lock_task;
    int _master_lock_count;
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
    SpinlockIRQ _task_lock;
    void process_pending(RouterThread*);

    // TIMERS
    Vector<Timer*> _timer_heap;
#if CLICK_LINUXMODULE
    spinlock_t _timer_lock;
    struct task_struct *_timer_task;
#endif
    void lock_timers();
    bool attempt_lock_timers();
    void unlock_timers();
    void timer_reheapify_from(int, Timer*, bool will_delete);

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
    simclick_sim _siminst;
    simclick_click _clickinst;
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
#endif
}

inline void
Master::unpause()
{
    _master_paused--;
}

inline void
Master::lock_timers()
{
#if CLICK_LINUXMODULE
    if (current != _timer_task)
    	spin_lock(&_timer_lock);
#endif
}

inline bool
Master::attempt_lock_timers()
{
#if CLICK_LINUXMODULE
    return spin_trylock(&_timer_lock);
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
#endif
}

CLICK_ENDDECLS
#endif
