// -*- c-basic-offset: 2; related-file-name: "../../lib/routerthread.cc" -*-
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
  int thread_id() const			{ return _id; }

  void driver();
  void driver_once();

  // Task list functions
  bool empty() const;

  void lock_tasks();
  bool attempt_lock_tasks();
  void unlock_tasks();

  void unschedule_all_tasks();

 private:

  Router *_router;
  int _id;

  Spinlock _lock;
  uatomic32_t _task_lock_waiting;
  uatomic32_t _pending;

  void set_thread_id(int i)		{ _id = i; }

#ifdef CLICK_BSDMODULE
  // XXX FreeBSD
  Task *_wakeup_list;
#endif

  // task request IDs
  void add_pending()			{ _pending++; }
  void process_task_requests();

  inline void nice_lock_tasks();
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
