/*
 * discardnotify.{cc,hh} -- element throws away all packets
 * Alexander Yip
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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
#include "discardnotify.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/routervisitor.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

DiscardNotify::DiscardNotify() :
  Discard()
{
}

DiscardNotify::~DiscardNotify()
{
}


int
DiscardNotify::initialize(ErrorHandler *errh)
{
  int ok, i;

  Discard::initialize(errh);
  _data_ready = false;

  ElementCastTracker filter(router(), "QueueNotify");
  ok = router()->visit_upstream(this, 0, &filter);
  if (ok < 0)
    return errh->error("could not find upstream notify queues");

  for(i=0; i<filter.size(); i++) {
    ((QueueNotify*) filter[i])->subscribe_notification(this);
  }

  return 0;
}

void
DiscardNotify::notify(int signal)
{
  if (signal == QueueNotify::NODATA)
    _data_ready = false;
  else if (signal == QueueNotify::DATAREADY){
    _data_ready = true;
    _task.fast_reschedule();
  }
}


bool
DiscardNotify::run_task(Task *)
{
  Packet *p = input(0).pull();
  if (p)
    p->kill();

  if (_data_ready)
    _task.fast_reschedule();
  return p != 0;
}



EXPORT_ELEMENT(DiscardNotify)
