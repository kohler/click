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
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

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
  configuration(conf);

  for (int i = 0; i < conf.size(); i++) {
    Element *e;
    int hi;
    if (cp_handler(conf[i] + ".scheduled", this, true, false, &e, &hi, errh)) {
      _e.push_back(e);
      _handlers.push_back(hi);
    }
  }

  if (_e.size() < conf.size())
    return -1;

  _timer.initialize(this);
  _timer.schedule_after_ms(10);
  return 0;
}

void
QuitWatcher::run_timer()
{
  String unscheduled_string = "false\n";
  for (int i = 0; i < _e.size(); i++) {
    const Router::Handler *h = router()->handler(_handlers[i]);
    String s = h->call_read(_e[i]);
    if (s == unscheduled_string) {
      router()->please_stop_driver();
      return;
    }
  }
  
  _timer.schedule_after_ms(10);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(QuitWatcher)
