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
#include <click/config.h>
#include <click/package.hh>
#include "quitwatcher.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include "elements/standard/scheduleinfo.hh"

QuitWatcher::QuitWatcher()
  : _timer(this)
{
  MOD_INC_USE_COUNT;
}

QuitWatcher::~QuitWatcher()
{
  MOD_DEC_USE_COUNT;
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
QuitWatcher::initialize(ErrorHandler *)
{
  _timer.attach(this);
  _timer.schedule_after_ms(2);
  return 0;
}

void
QuitWatcher::uninitialize()
{
  _timer.unschedule();
}

void
QuitWatcher::run_scheduled()
{
  for (int i = 0; i < _e.size(); i++)
    if (!_e[i]->scheduled()) {
      router()->please_stop_driver();
      return;
    }
  _timer.schedule_after_ms(2);
}

EXPORT_ELEMENT(QuitWatcher)
