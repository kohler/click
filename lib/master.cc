// -*- c-basic-offset: 4; related-file-name: "../include/click/master.hh" -*-
/*
 * master.{cc,hh} -- Click event master
 * Eddie Kohler
 *
 * Copyright (c) 2003-7 The Regents of the University of California
 * Copyright (c) 2010 Intel Corporation
 * Copyright (c) 2008-2011 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/master.hh>
#include <click/element.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/handlercall.hh>
#include <click/heap.hh>
#if CLICK_USERLEVEL
# include <fcntl.h>
# include <click/userutils.hh>
#endif
CLICK_DECLS

#if CLICK_USERLEVEL
volatile sig_atomic_t Master::signals_pending;
static volatile sig_atomic_t signal_pending[NSIG];
static RouterThread *signal_thread;
extern "C" { static void sighandler(int signo); }
#endif

Master::Master(int nthreads)
    : _routers(0)
{
    _refcount = 0;
    _master_paused = 0;

    _nthreads = nthreads + 1;
    _threads = new RouterThread *[_nthreads];
    for (int tid = -1; tid < nthreads; tid++)
        _threads[tid + 1] = new RouterThread(this, tid);

#if CLICK_USERLEVEL
    // signal information
    signals_pending = 0;
    _siginfo = 0;
    sigemptyset(&_sig_dispatching);
    signal_thread = _threads[1];
#endif

#if CLICK_LINUXMODULE
    spin_lock_init(&_master_lock);
    _master_lock_task = 0;
    _master_lock_count = 0;
#endif

#if CLICK_NS
    _simnode = 0;
#endif
}

Master::~Master()
{
    lock_master();
    _refcount++;
    while (_routers) {
        Router *r = _routers;
        r->use();
        unlock_master();
        unregister_router(r);
        r->unuse();
        lock_master();
    }
    _refcount--;
    unlock_master();

    if (_refcount > 0)
        click_chatter("deleting master while ref count = %d", _refcount);

#if CLICK_USERLEVEL
    signal_thread = 0;
#endif
    for (int i = 0; i < _nthreads; i++)
        delete _threads[i];
    delete[] _threads;
}

void
Master::use()
{
    lock_master();
    _refcount++;
    unlock_master();
}

void
Master::unuse()
{
    lock_master();
    _refcount--;
    bool del = (_refcount <= 0);
    unlock_master();
    if (del)
        delete this;
}

void
Master::pause()
{
    _master_paused++;
    for (int i = 1; i < _nthreads; ++i) {
        _threads[i]->timer_set().fence();
#if CLICK_USERLEVEL
        _threads[i]->select_set().fence();
#endif
    }
}

void
Master::block_all()
{
    for (int i = 1; i < _nthreads; ++i)
        _threads[i]->schedule_block_tasks();
    for (int i = 1; i < _nthreads; ++i)
        _threads[i]->block_tasks(true);
    pause();
}

void
Master::unblock_all()
{
    unpause();
    for (int i = 1; i < _nthreads; ++i)
        _threads[i]->unblock_tasks();
}


// ROUTERS

void
Master::register_router(Router *router)
{
    lock_master();
    assert(router && router->_master == 0 && router->_running == Router::RUNNING_INACTIVE && !router->_next_router);
    _refcount++;                // balanced in unregister_router()
    router->_master = this;
    router->_next_router = _routers;
    _routers = router;
    unlock_master();
}

void
Master::prepare_router(Router *router)
{
    // increments _master_paused; should quickly call run_router() or
    // kill_router()
    lock_master();
    assert(router && router->_master == this && router->_running == Router::RUNNING_INACTIVE);
    router->_running = Router::RUNNING_PREPARING;
    unlock_master();
    pause();
}

void
Master::run_router(Router *router, bool foreground)
{
    lock_master();
    assert(router && router->_master == this && router->_running == Router::RUNNING_PREPARING);
    router->_running = (foreground ? Router::RUNNING_ACTIVE : Router::RUNNING_BACKGROUND);
    unlock_master();
    unpause();
}

void
Master::kill_router(Router *router)
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif

    lock_master();
    assert(router && router->_master == this);
    int was_running = router->_running;
    router->_running = Router::RUNNING_DEAD;
    if (was_running >= Router::RUNNING_BACKGROUND)
        pause();
    else if (was_running == Router::RUNNING_PREPARING)
        /* nada */;
    else {
        /* could not have anything on the list */
        assert(was_running == Router::RUNNING_INACTIVE || was_running == Router::RUNNING_DEAD);
        unlock_master();
        return;
    }

    // Fix stopper
    request_stop();
#if CLICK_LINUXMODULE
    preempt_disable();
#endif
    unlock_master();

    // Remove tasks
    for (RouterThread **tp = _threads; tp != _threads + _nthreads; ++tp)
        (*tp)->kill_router(router);

    // 4.Sep.2007 - Don't bother to remove pending tasks.  They will be
    // removed shortly anyway, either when the task itself is deleted or (more
    // likely) when the pending list is processed.

#if CLICK_USERLEVEL
    // Remove signals
    {
        _signal_lock.acquire();
        SignalInfo **pprev = &_siginfo;
        for (SignalInfo *si = *pprev; si; si = *pprev)
            if (si->router == router) {
                remove_signal_handler(si->signo, si->router, si->handler);
                pprev = &_siginfo;
            } else
                pprev = &si->next;
        _signal_lock.release();
    }
#endif

    unpause();
#if CLICK_LINUXMODULE
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    preempt_enable();
#  else
    preempt_enable_no_resched();
#  endif
#endif

    // something has happened, so wake up threads
    for (RouterThread **tp = _threads + 1; tp != _threads + _nthreads; ++tp)
        (*tp)->wake();
}

void
Master::unregister_router(Router *router)
{
    assert(router);
    lock_master();

    if (router->_master) {
        assert(router->_master == this);

        if (router->_running >= Router::RUNNING_PREPARING)
            kill_router(router);

        Router **pprev = &_routers;
        for (Router *r = *pprev; r; r = r->_next_router)
            if (r != router) {
                *pprev = r;
                pprev = &r->_next_router;
            }
        *pprev = 0;
        _refcount--;            // balanced in register_router()
        router->_master = 0;
    }

    unlock_master();
}

bool
Master::check_driver()
{
#if CLICK_LINUXMODULE
    assert(!in_interrupt());
#endif

    lock_master();
    request_go();

    for (Router *r = _routers; r; ) {
        Router *next_router = r->_next_router;
        if (r->runcount() <= 0 && r->_running >= Router::RUNNING_BACKGROUND) {
            Element *dm = (Element *)(r->attachment("Script"));
            if (dm) {
                int max = 1000;
                while (HandlerCall::call_write(dm, "step", "router", ErrorHandler::default_handler()) == 0
                       && r->runcount() <= 0 && --max >= 0)
                    /* do nothing */;
            }
            if (r->runcount() <= 0 && r->_running >= Router::RUNNING_BACKGROUND)
                kill_router(r);
        }
        r = next_router;
    }

    bool any_active = false;
    for (Router *r = _routers; r; r = r->_next_router)
        if (r->_running == Router::RUNNING_ACTIVE) {
            any_active = true;
            break;
        }
    if (!any_active)
        request_stop();
    unlock_master();
    return any_active;
}


// SIGNALS

#if CLICK_USERLEVEL

inline void
Master::signal_handler(int signo)
{
    signals_pending = signal_pending[signo] = 1;
# if HAVE_MULTITHREAD
    click_fence();
# endif
    if (signal_thread)
        signal_thread->wake();
}

extern "C" {
static void
sighandler(int signo)
{
    Master::signal_handler(signo);
}
}

int
Master::add_signal_handler(int signo, Router *router, String handler)
{
    if (signo < 0 || signo >= NSIG || router->master() != this)
        return -1;

    _signal_lock.acquire();
    int status = 0, nhandlers = 0;
    SignalInfo **pprev = &_siginfo;
    for (SignalInfo *si = *pprev; si; si = *pprev)
        if (si->equals(signo, router, handler))
            goto unlock_exit;
        else {
            nhandlers += (si->signo == signo);
            pprev = &si->next;
        }

    if ((*pprev = new SignalInfo(signo, router, handler))) {
        if (nhandlers == 0 && sigismember(&_sig_dispatching, signo) == 0)
            click_signal(signo, sighandler, false);
    } else
        status = -1;

  unlock_exit:
    _signal_lock.release();
    return status;
}

int
Master::remove_signal_handler(int signo, Router *router, String handler)
{
    _signal_lock.acquire();
    int nhandlers = 0, status = -1;
    SignalInfo **pprev = &_siginfo;
    for (SignalInfo *si = *pprev; si; si = *pprev)
        if (si->equals(signo, router, handler)) {
            *pprev = si->next;
            delete si;
            status = 0;
        } else {
            nhandlers += (si->signo == signo);
            pprev = &si->next;
        }

    if (status >= 0 && nhandlers == 0
        && sigismember(&_sig_dispatching, signo) == 0)
        click_signal(signo, SIG_DFL, false);

    _signal_lock.release();
    return status;
}

void
Master::process_signals(RouterThread *thread)
{
    thread->set_thread_state(RouterThread::S_RUNSIGNAL);

    // grab the signal lock
    signals_pending = 0;
    _signal_lock.acquire();

    // collect activated signal handler info
    SignalInfo *happened, **hpprev = &happened;
    for (SignalInfo **pprev = &_siginfo, *si = *pprev; si; si = *pprev)
        if ((signal_pending[si->signo]
             || sigismember(&_sig_dispatching, si->signo) > 0)
            && si->router->running()) {
            sigaddset(&_sig_dispatching, si->signo);
            signal_pending[si->signo] = 0;
            *pprev = si->next;
            *hpprev = si;
            hpprev = &si->next;
        } else
            pprev = &si->next;
    *hpprev = 0;

    // call relevant signal handlers
    sigset_t sigset_handled;
    sigemptyset(&sigset_handled);
    while (happened) {
        SignalInfo *next = happened->next;
        if (HandlerCall::call_write(happened->handler, happened->router->root_element()) >= 0)
            sigaddset(&sigset_handled, happened->signo);
        delete happened;
        happened = next;
    }

    // collect currently active signal handlers (handler calls may have
    // changed this set)
    sigset_t sigset_active;
    sigemptyset(&sigset_active);
    for (SignalInfo *si = _siginfo; si; si = si->next)
        sigaddset(&sigset_active, si->signo);

    // reset & possibly redeliver unhandled signals and signals that we gave
    // up on that happened again since we started running this function
    for (int signo = 0; signo < NSIG; ++signo)
        if (sigismember(&_sig_dispatching, signo) > 0) {
            if (sigismember(&sigset_active, signo) == 0) {
                click_signal(signo, SIG_DFL, false);
                click_fence();
                if (signal_pending[signo] != 0) {
                    signal_pending[signo] = 0;
                    goto suicide;
                }
            }
            if (sigismember(&sigset_handled, signo) == 0) {
            suicide:
                kill(getpid(), signo);
            }
        }

    sigemptyset(&_sig_dispatching);
    _signal_lock.release();
}

#endif


// NS

#if CLICK_NS

void
Master::initialize_ns(simclick_node_t *simnode)
{
    assert(!_simnode);
    _simnode = simnode;
}

#endif


#if CLICK_DEBUG_MASTER || CLICK_DEBUG_SCHEDULING
#include <click/straccum.hh>

String
Master::info() const
{
    StringAccum sa;
    sa << "paused:\t\t" << _master_paused << '\n';
    sa << "stop_flag:\t" << _threads[0]->_stop_flag << '\n';
    for (int i = 0; i < _nthreads; i++) {
        RouterThread *t = _threads[i];
        sa << "thread " << (i - 1) << ":";
# ifdef CLICK_LINUXMODULE
        if (t->_sleeper)
            sa << "\tsleep";
        else
            sa << "\twake";
# endif
        if (t->_pending_head.x)
            sa << "\tpending";
# if CLICK_USERLEVEL
        if (t->select_set()._wake_pipe[0] >= 0) {
            fd_set rfd;
            struct timeval to;
            FD_ZERO(&rfd);
            FD_SET(t->select_set()._wake_pipe[0], &rfd);
            timerclear(&to);
            (void) select(t->select_set()._wake_pipe[0] + 1, &rfd, 0, 0, &to);
            if (FD_ISSET(t->select_set()._wake_pipe[0], &rfd))
                sa << "\tpipewoken";
        }
# endif
# if CLICK_DEBUG_SCHEDULING
        sa << '\t' << RouterThread::thread_state_name(t->thread_state());
# endif
        sa << '\n';
# if CLICK_DEBUG_SCHEDULING > 1
        t->set_thread_state(t->thread_state()); // account for time
        bool any = false;
        for (int s = 0; s < RouterThread::NSTATES; ++s)
            if (Timestamp time = t->thread_state_time(s)) {
                sa << (any ? ", " : "\t\t")
                   << RouterThread::thread_state_name(s)
                   << ' ' << time << '/' << t->thread_state_count(s);
                any = true;
            }
        if (any)
            sa << '\n';
# endif
    }
    return sa.take_string();
}

#endif
CLICK_ENDDECLS
