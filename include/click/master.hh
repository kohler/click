// -*- c-basic-offset: 4; related-file-name: "../../lib/master.cc" -*-
#ifndef CLICK_MASTER_HH
#define CLICK_MASTER_HH
#include <click/vector.hh>
#include <click/timer.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
# if HAVE_POLL_H
#  include <poll.h>
# endif
#endif
CLICK_DECLS
class Element;

class Master { public:

    Master();
    ~Master();

    TimerList *timer_list()		{ return &_timer_list; }
    
#if CLICK_USERLEVEL
    int add_select(int fd, Element *, int mask);
    int remove_select(int fd, Element *, int mask);
    void run_selects(bool more_tasks);
#endif

  private:

    TimerList _timer_list;
    
#if CLICK_USERLEVEL
# if !HAVE_POLL_H
    struct pollfd {
	int fd;
	int events;
    };
    enum { POLLIN = SELECT_READ, POLLOUT = SELECT_WRITE };
    fd_set _read_select_fd_set;
    fd_set _write_select_fd_set;
    int _max_select_fd;
# endif
    Vector<struct pollfd> _pollfds;
    Vector<Element *> _read_poll_elements;
    Vector<Element *> _write_poll_elements;
    Spinlock _wait_lock;
#endif

};

CLICK_ENDDECLS
#endif
