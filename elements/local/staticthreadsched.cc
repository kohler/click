#include <click/config.h>
#include <click/package.hh>
#include "../standard/scheduleinfo.hh"
#include "staticthreadsched.hh"
#include <click/task.hh>
#include <click/routerthread.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>

StaticThreadSched::StaticThreadSched()
  : _timer(this)
{
  MOD_INC_USE_COUNT;
}

StaticThreadSched::~StaticThreadSched()
{
  MOD_DEC_USE_COUNT;
}

int
StaticThreadSched::configure(const Vector<String> &conf, ErrorHandler *errh)
{
#if __MTCLICK__
  for (int i = 0; i < conf.size(); i++) {
    Vector<String> parts;
    int thread;
    cp_spacevec(conf[i], parts);
    if (parts.size() == 0) /* empty argument OK */;
    else if (parts.size() != 2 || !cp_integer(parts[1], &thread))
      return errh->error("expected `ELEMENTNAME THREAD', got `%s'", 
	                 String(conf[i]).cc());
    else {
      _element_names.push_back(parts[0]);
      _threads.push_back(thread);
    }
  }
  return 0;
#else
  return errh->error("StaticThreadSched requires multithreading\n");
#endif
}

int
StaticThreadSched::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(1000);
  return 0;
}

void
StaticThreadSched::run_scheduled()
{
#if __MTCLICK__
  TaskList *task_list = router()->task_list();
  task_list->lock();
  Task *t = task_list->initialized_next();
  while (t != task_list) {
    Element *e = t->element();
    if (e) {
      for (int i=0; i<_element_names.size(); i++) {
        if (_element_names[i] == e->id()) {
          if (_threads[i] < router()->nthreads()) {
            click_chatter("sticking element %s on thread %d", 
	                  e->id().cc(), _threads[i]);
            int old = t->thread_preference();
            if (old >= 0 && old != _threads[i]) {
	      t->set_thread_preference(_threads[i]);
              router()->thread(old)->add_task_request
		(RouterThread::MOVE_TASK, t);
            } else if (old < 0)
	      router()->thread(_threads[i])->add_task_request
		(RouterThread::SCHEDULE_TASK, t);
	  }
	  break;
	}
      }
    }
    t = t->initialized_next();
  }
  task_list->unlock();
#endif
}

EXPORT_ELEMENT(StaticThreadSched)

