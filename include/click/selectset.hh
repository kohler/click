// -*- related-file-name: "../../lib/selectset.cc" -*-
#ifndef CLICK_SELECTSET_HH
#define CLICK_SELECTSET_HH 1
#if !CLICK_USERLEVEL
# error "<click/selectset.hh> only meaningful at user level"
#endif
#include <click/vector.hh>
#include <click/sync.hh>
#include <unistd.h>
#if !HAVE_ALLOW_SELECT && !HAVE_ALLOW_POLL && !HAVE_ALLOW_KQUEUE
# define HAVE_ALLOW_SELECT 1
#endif
#if defined(__APPLE__) && HAVE_ALLOW_SELECT && HAVE_ALLOW_POLL
// Apple's poll() is often broken
# undef HAVE_ALLOW_POLL
#endif
#if HAVE_POLL_H && HAVE_ALLOW_POLL
# include <poll.h>
#else
# undef HAVE_ALLOW_POLL
# if !HAVE_ALLOW_SELECT && !HAVE_ALLOW_KQUEUE
#  error "poll is not supported on this system, try --enable-select"
# endif
#endif
#if !HAVE_SYS_EVENT_H || !HAVE_KQUEUE
# undef HAVE_ALLOW_KQUEUE
# if !HAVE_ALLOW_SELECT && !HAVE_ALLOW_POLL
#  error "kqueue is not supported on this system, try --enable-select"
# endif
#endif
CLICK_DECLS
class Element;
class Router;
class RouterThread;

class SelectSet { public:

    SelectSet();
    ~SelectSet();

    void initialize(RouterThread *thread);

    int add_select(RouterThread *thread, int fd, Element *element, int mask);
    int remove_select(int fd, Element *element, int mask);

    void run_selects(RouterThread *thread);

    void kill_router(Router *router);

    inline void fence();

  private:

    struct SelectorInfo {
	Element *read;
	Element *write;
	int pollfd;
	SelectorInfo()
	    : read(0), write(0), pollfd(-1) {
	}
    };

    int _wake_pipe[2];
#if HAVE_ALLOW_KQUEUE
    int _kqueue;
#endif
#if !HAVE_ALLOW_POLL
    struct pollfd {
	int fd;
	int events;
    };
    fd_set _read_select_fd_set;
    fd_set _write_select_fd_set;
    int _max_select_fd;
#endif /* !HAVE_ALLOW_POLL */
    Vector<struct pollfd> _pollfds;
    Vector<SelectorInfo> _selinfo;
#if HAVE_MULTITHREAD
    SimpleSpinlock _select_lock;
    click_processor_t _select_processor;
#endif

    void register_select(int fd, bool add_read, bool add_write);
    void remove_pollfd(int pi, int event);
    inline void call_selected(int fd, int mask) const;
    inline bool post_select(RouterThread *thread, bool was_active, bool acquire);
#if HAVE_ALLOW_KQUEUE
    void run_selects_kqueue(RouterThread *thread);
#endif
#if HAVE_ALLOW_POLL
    void run_selects_poll(RouterThread *thread);
#else
    void run_selects_select(RouterThread *thread);
#endif

    inline void lock();
    inline void unlock();

#if CLICK_DEBUG_MASTER || CLICK_DEBUG_SCHEDULING
    friend class Master;
#endif

};

inline void
SelectSet::lock()
{
#if HAVE_MULTITHREAD
    if (click_get_processor() != _select_processor)
	_select_lock.acquire();
#endif
}

inline void
SelectSet::unlock()
{
#if HAVE_MULTITHREAD
    if (click_get_processor() != _select_processor)
	_select_lock.release();
#endif
}

inline void
SelectSet::fence()
{
    lock();
    unlock();
}

CLICK_ENDDECLS
#endif
