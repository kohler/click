/*
 * quitwatcher.{cc,hh} -- element quits driver when other elements are not
 * scheduled any more
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "quitwatcher.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"
#include "elements/standard/scheduleinfo.hh"

QuitWatcher::QuitWatcher()
{
}

int
QuitWatcher::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    Element *e = router()->find(this, conf[i], errh);
    if (e) _e.push_back(e);
  }
  return (errh->nerrors() == before ? 0 : -1);
}

int
QuitWatcher::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
QuitWatcher::uninitialize()
{
  unschedule();
}

void
QuitWatcher::run_scheduled()
{
  for (int i = 0; i < _e.size(); i++)
    if (!_e[i]->scheduled()) {
      router()->please_stop_driver();
      return;
    }
  reschedule();
}

EXPORT_ELEMENT(QuitWatcher)
