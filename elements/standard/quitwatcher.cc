/*
 * quitwatcher.{cc,hh} -- element quits driver when other elements are not
 * scheduled any more
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
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
