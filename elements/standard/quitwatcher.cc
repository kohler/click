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
  _timer.initialize(this);
  _timer.schedule_after_ms(10);
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
  // collect handlers
  if (_handlers.size() < _e.size()) {
    for (int i = 0; i < _e.size(); i++) {
      int hi = router()->find_handler(_e[i], "scheduled");
      if (hi < 0)
	click_chatter("%s: %s has no `scheduled' handler", id().cc(), _e[i]->id().cc());
      else {
	const Router::Handler &h = router()->handler(hi);
	if (!h.read)
	  click_chatter("%s: `%s.scheduled' is a write handler", id().cc(), _e[i]->id().cc());
	else
	  _handlers.push_back(hi);
      }
    }
    if (_handlers.size() < _e.size())
      return;
  }

  String unscheduled_string = "false\n";
  for (int i = 0; i < _e.size(); i++) {
    const Router::Handler &h = router()->handler(_handlers[i]);
    String s = h.read(_e[i], h.read_thunk);
    if (s == unscheduled_string) {
      router()->please_stop_driver();
      return;
    }
  }
  
  _timer.schedule_after_ms(10);
}

EXPORT_ELEMENT(QuitWatcher)
