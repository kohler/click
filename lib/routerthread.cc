#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/glue.hh>
#include <click/router.hh>
#include <click/routerthread.hh>
#ifdef __KERNEL__
extern "C" {
#include <linux/sched.h>
}
#endif

#define DEBUG_RT_SCHED 0

RouterThread::RouterThread(Router *r)
  : Task(Task::error_hook, 0), _router(r), _please_stop_driver(0)
{
  _prev = _next = _list = this;
  router()->add_thread(this);
  // add_thread() will call this->set_thread_id()
}

RouterThread::~RouterThread()
{
  router()->remove_thread(this);
}

void
RouterThread::add_task_request(unsigned op, Task *t)
{
  _taskreq_lock.acquire();
  _taskreq_ops.push_back(op);
  _taskreq_tasks.push_back(t);
  _taskreq_lock.release();
}

inline void
RouterThread::process_task_requests()
{
  Vector<unsigned> ops;
  Vector<Task *> tasks;

  if (_taskreq_ops.size() == 0)
    return;

  _taskreq_lock.acquire();
  ops.swap(_taskreq_ops);
  tasks.swap(_taskreq_tasks);
  _taskreq_lock.release();

  for (int i = 0; i < ops.size(); i++) {
    Task *t = tasks[i];
    switch (ops[i]) {
      
     case SCHEDULE_TASK:
      if (t->scheduled_list() == this)
	t->reschedule();
      break;
      
     case UNSCHEDULE_TASK:
      if (t->scheduled() && t->scheduled_list() == this) 
	t->fast_unschedule();
      break;

#if __MTCLICK__
     case MOVE_TASK:
      if (t->scheduled_list() == this)
	t->fast_change_thread();
      break;
#endif

    }
  }
}

#if __MTCLICK__

void
RouterThread::driver()
{
  unsigned long long cycles = 0;
  lock_tasks();
  while (1) {
    int c = MAX_DRIVER_COUNT;
    while (c >= 0) {
      if (_please_stop_driver) {
	unlock_tasks();
	return;
      }
      process_task_requests();
      Task *t = scheduled_next();
      if (t != this) {
	int runs = t->fast_unschedule();
	if (runs > PROFILE_ELEMENT)
	  cycles = click_get_cycles();
	t->call_hook();
	if (runs > PROFILE_ELEMENT) {
          cycles = click_get_cycles() - cycles;
	  cycles = ((unsigned)cycles)/32 + ((unsigned)t->cycles())*31/32;
	  t->update_cycles(cycles);
	}
      }
      c--;
    }
    wait();
  }
  assert(0);
}

#else

void
RouterThread::driver()
{
  Task *t;
  lock_tasks();
  while (1) {
    int c = MAX_DRIVER_COUNT;
    while ((t = scheduled_next()),
           t != this && !_please_stop_driver && c >= 0) {
      t->unschedule();
      t->call_hook();
      c--;
    }
    if (_please_stop_driver)
      break;
    else
      wait();
  }
  unlock_tasks();
}

#endif

void
RouterThread::driver_once()
{
  if (_please_stop_driver)
    return;
  lock_tasks();
  Task *t = scheduled_next();
  if (t != this) {
    t->unschedule();
    t->call_hook();
  }
  unlock_tasks();
}

void
RouterThread::wait()
{
  unlock_tasks();
#if CLICK_USERLEVEL
  router()->run_selects(!empty());
#else /* __KERNEL__ */
  schedule();
#endif
  lock_tasks();
  router()->run_timers();
}
