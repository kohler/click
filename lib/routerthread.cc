#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "router.hh"
#include "elemlink.hh"
#include "routerthread.hh"

RouterThread::RouterThread(Router *r)
  : _router(r), _please_stop_driver(0)
{
  router()->use();
  router()->add_thread(this);
}

RouterThread::~RouterThread()
{
  router()->remove_thread(this);
  router()->unuse();
}


void
RouterThread::wait()
{
#ifndef __KERNEL__
  router()->wait_in_select();
#else /* __KERNEL__ */
  schedule();
#endif
  router()->run_timers();
}

void
RouterThread::driver()
{
  ElementLink *l;
  while (1) {
    int c = _max_driver_count;
    while (c >= 0) {
      router()->worklist_lock_acquire();
      if (_please_stop_driver) {
        router()->worklist_lock_release();
        return;
      }
      l = router()->scheduled_next();
      if (l != router()) {
	l->unschedule();
	router()->worklist_lock_release();
	((Element *)l)->run_scheduled();
      } 
      else 
	router()->worklist_lock_release();
      c--;
    }
    wait();
  }
}

void
RouterThread::driver_once()
{
  if (_please_stop_driver)
    return;
  router()->worklist_lock_acquire();
  ElementLink *l = router()->scheduled_next();
  if (l != router()) {
    l->unschedule();
    router()->worklist_lock_release();
    ((Element *)l)->run_scheduled();
  } 
  else 
    router()->worklist_lock_release();
}


