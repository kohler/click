#ifndef ROUTERTHREAD_HH
#define ROUTERTHREAD_HH
#include <click/sync.hh>
#include <click/vector.hh>
#include <click/ewma.hh>
// NB: user must #include <click/task.hh> before <click/routerthread.hh>.
// We cannot #include <click/task.hh> ourselves because of circular #include
// dependency.

class RouterThread : public Task { public:

  RouterThread(Router*);
  ~RouterThread();
  
  Router *router() const		{ return _router; }
  int thread_id() const			{ return _id; }

  void driver();
  void driver_once();
  void wait();

  // Task list functions
  bool empty() const;

  void lock_tasks();
  bool attempt_lock_tasks();
  void unlock_tasks();

  void add_task_request(unsigned, Task *);
  void process_task_requests();

  // task request IDs
  static const unsigned SCHEDULE_TASK = 0;
  static const unsigned UNSCHEDULE_TASK = 1;
#if __MTCLICK__
  // Unschedule a task on this task list and move it to another thread. Call
  // MOVE_TASK on the original thread; pass the Task with thread_preference()
  // set to the destination thread.
  static const unsigned MOVE_TASK = 2;
#endif
  
 private:
  
#if CLICK_USERLEVEL
  static const int MAX_DRIVER_COUNT = 5000;
#else
  static const int MAX_DRIVER_COUNT = 10000;
#endif
  static const int PROFILE_ELEMENT = 200;

  Router *_router;
  int _id;
  bool _please_stop_driver;

  Spinlock _task_lock;
  
  Spinlock _taskreq_lock;
  Vector<unsigned> _taskreq_ops;
  Vector<Task *> _taskreq_tasks;

  void set_thread_id(int i)		{ _id = i; }
  void please_stop_driver()		{ _please_stop_driver = 1; }

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
  _task_lock.acquire();
}

inline bool
RouterThread::attempt_lock_tasks()
{
  return _task_lock.attempt();
}

inline void
RouterThread::unlock_tasks()
{
  _task_lock.release();
}

#endif
