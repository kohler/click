/*
 * readhandler.{cc,hh} -- element that dumps output of read handlers
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "confparse.hh"
#include "readhandler.hh"
#include "../standard/scheduleinfo.hh"
 
int
ReadHandlerCaller::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "approximate duration between run", &_n,
		  cpEnd) < 0)
    return -1;
  return 0;
}

int
ReadHandlerCaller::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  _last_time = click_jiffies();
  return 0;
}
  
void
ReadHandlerCaller::run_scheduled()
{
  int jiffs = click_jiffies();
  if ((jiffs-_last_time)/CLICK_HZ > _n) {
    extern int call_read_handlers();
    call_read_handlers();
    _last_time = jiffs;
  }
  reschedule();
}

EXPORT_ELEMENT(ReadHandlerCaller)

