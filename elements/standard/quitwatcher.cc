/*
 * quitwatcher.{cc,hh} -- element quits driver when other elements are not
 * scheduled any more
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "quitwatcher.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/handlercall.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

QuitWatcher::QuitWatcher()
  : _timer(this)
{
}

int
QuitWatcher::configure(Vector<String> &, ErrorHandler *)
{
  // always succeed
  return 0;
}

int
QuitWatcher::initialize(ErrorHandler *errh)
{
  // parse configuration arguments now, because handlers are available
  Vector<String> conf;
  cp_argvec(configuration(), conf);

  for (int i = 0; i < conf.size(); i++) {
    HandlerCall hc(conf[i] + ".scheduled");
    if (hc.initialize_read(this, errh) >= 0) {
      _e.push_back(hc.element());
      _handlers.push_back(hc.handler());
    }
  }

  if (_e.size() < conf.size())
    return -1;

  _timer.initialize(this);
  _timer.schedule_after_msec(10);
  return 0;
}

void
QuitWatcher::run_timer(Timer *)
{
    for (int i = 0; i < _e.size(); i++) {
	String s = _handlers[i]->call_read(_e[i]);
	if (!s.length() || s[0] == 'f') {
	    router()->please_stop_driver();
	    return;
	}
    }
    _timer.schedule_after_msec(10);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(QuitWatcher)
