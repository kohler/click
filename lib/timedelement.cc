#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "timedelement.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

TimedElement::TimedElement()
  : _timer(this), _interval_ms(-1)
{
}

TimedElement::~TimedElement()
{
  assert(!timer_scheduled());
}

int
TimedElement::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, router, errh,
		     cpInterval, "time interval in seconds", &_interval_ms,
		     0);
}

void
TimedElement::uninitialize(Router *)
{
  timer_unschedule();
}
