#include <click/config.h>
#include <click/package.hh>
#include "../standard/scheduleinfo.hh"
#include "binpackingsched.hh"
#include <click/task.hh>
#include <click/routerthread.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/task.hh>
#include <click/router.hh>
#include <click/error.hh>

#define DEBUG 1

BinPackingScheduler::BinPackingScheduler()
  : _timer(this)
{
  MOD_INC_USE_COUNT;
}

BinPackingScheduler::~BinPackingScheduler()
{
  MOD_DEC_USE_COUNT;
}

int
BinPackingScheduler::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(1000);
  return 0;
}
  
int 
BinPackingScheduler::configure(const Vector<String> &conf, ErrorHandler *errh)
{
#if __MTCLICK__
  _interval = 1000;
  if (cp_va_parse(conf, this, errh, 
	          cpOptional,
	          cpUnsigned, "interval", &_interval, 0) < 0)
    return -1;
  return 0;
#else
  return errh->error("BinPackingScheduler requires multithreading\n");
#endif
}

void
BinPackingScheduler::run_scheduled()
{
#if __MTCLICK__
  Vector<Task*> tasks;
  unsigned total_load = 0;
  unsigned avg_load;
  TaskList *task_list = router()->task_list();
  task_list->lock();
  Task *t = task_list->initialized_next();
  while (t != task_list) {
    total_load += t->cycles();
    tasks.push_back(t);
    t = t->initialized_next();
  }
  task_list->unlock();
  
#if DEBUG > 0
  int print = 0;
  int high = 0;
#endif

  // slow sorting algorithm, but works okay for small number of tasks
  Vector<Task*> sorted;
  for (int i=0; i<tasks.size(); i++) {
    int max = 0;
    int which = -1;
    for (int j=0; j<tasks.size(); j++) {
      if (tasks[j] && tasks[j]->cycles() > max) { 
	which = j; 
	max = tasks[j]->cycles();
      }
    }
    assert(which >= 0);
    sorted.push_back(tasks[which]);
    tasks[which] = 0;
#if DEBUG > 0
    if (i == 0) high = max;
#endif
  }

#if DEBUG > 0
  if (high >= 500) {
    print = 1;
    unsigned now = click_jiffies();
    for(int i=0; i<sorted.size(); i++) {
	Element *e = sorted[i]->element();
	if (e && 0)
	  click_chatter("%u: %s %d, was on %d", 
	                now, e->id().cc(), 
			sorted[i]->cycles(), 
			sorted[i]->thread_preference());
    }
  }
#endif

  int n = router()->nthreads();
  int load[n];
  Vector<Task*> schedule[n];
  avg_load = total_load / n;

  for(int i=0; i<n; i++) load[i] = 0;
  // for(int i=0; i<sorted.size(); i++) {
  for(int i=sorted.size()-1; i>=0; i--) {
    int min = -1;
    int which = 0;
    
    for (int j=0; j<n; j++) {
      if (load[j] < min || min < 0) {
	which = j;
	min = load[j];
      }
    }

    load[which] += sorted[i]->cycles();
    schedule[which].push_back(sorted[i]);
    int old = sorted[i]->thread_preference();
    if (old >= 0 && old != which) {
      sorted[i]->set_thread_preference(which);
      router()->thread(old)->add_task_request
	(RouterThread::MOVE_TASK, sorted[i]);
    } else if (old < 0) 
      router()->thread(which)->add_task_request
	(RouterThread::SCHEDULE_TASK, sorted[i]);
  }
  
#if DEBUG > 0
  if (print) {
    unsigned now = click_jiffies();
    for(int i=0; i<sorted.size(); i++) {
	Element *e = sorted[i]->element();
	if (e) 
	  click_chatter("%u: %s %d, now on %d (%d)", 
	                now, e->id().cc(), 
			sorted[i]->cycles(), 
			sorted[i]->thread_preference(), avg_load);
    }
    print = 0;
    click_chatter("\n");
  }
#endif

  _timer.schedule_after_ms(_interval);
#endif
}

EXPORT_ELEMENT(BinPackingScheduler)

