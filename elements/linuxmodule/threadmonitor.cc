#include <click/config.h>
#include "threadmonitor.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>

ThreadMonitor::ThreadMonitor()
  : _timer(this)
{
  MOD_INC_USE_COUNT;
}

ThreadMonitor::~ThreadMonitor()
{
  MOD_DEC_USE_COUNT;
}

int
ThreadMonitor::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(1000);
  return 0;
}

int 
ThreadMonitor::configure(Vector<String> &conf, ErrorHandler *errh)
{
#if __MTCLICK__
  _interval = 1000;
  _thresh = 1000;
  if (cp_va_parse(conf, this, errh, 
	          cpOptional,
	          cpUnsigned, "interval", &_interval,
	          cpUnsigned, "thresh", &_thresh, 0) < 0)
    return -1;
  return 0;
#else
  (void) conf;
  return errh->error("ThreadMonitor requires multithreading\n");
#endif
}

void
ThreadMonitor::run_scheduled()
{
#if __MTCLICK__
  int print = 0;
  Vector<Task *> schedule[router()->nthreads()];

  TaskList *task_list = router()->task_list();
  task_list->lock();
  Task *t = task_list->all_tasks_next();
  while (t != task_list) {
    int thread = t->thread_preference();
    if (thread <= 0) thread = 0;
    schedule[thread].push_back(t);
    if (t->cycles() > _thresh) print = 1;
    t = t->all_tasks_next();
  }
  task_list->unlock();

  if (print) {
    unsigned tnow = click_jiffies();
    for (int i=0; i<router()->nthreads(); i++) {
      for (int j=0; j<schedule[i].size(); j++) {
	Task *t = schedule[i][j]; 
	Element *e = t->element(); 
	if (e) 
	  click_chatter("%u: %s, cycles %d, on %d", 
	                tnow, e->id().cc(), 
		        t->cycles(), t->thread_preference());
	else
          click_chatter("%u: with hook, cycles %d, on %d", 
	                tnow, t->cycles(), t->thread_preference());
      }
    }
    click_chatter("\n");
  }

  _timer.schedule_after_ms(_interval);
#endif
}

EXPORT_ELEMENT(ThreadMonitor)

