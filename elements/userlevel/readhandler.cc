/*
 * readhandler.{cc,hh} -- element that dumps output of read handlers
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "readhandler.hh"

void
ReadHandlerCaller::run_scheduled()
{
  extern int call_read_handlers();
  call_read_handlers();
  unschedule();
}

EXPORT_ELEMENT(ReadHandlerCaller)

