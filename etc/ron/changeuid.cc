/*
 * changeuid.{cc,hh} -- Changes UID of click process
 * Alexander Yip
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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

#include <click/config.h>
#include "changeuid.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <sys/types.h>
#include <unistd.h>
#include <click/timer.hh>

ChangeUID::ChangeUID()
  : _timer(timer_hook, this)
{
}

ChangeUID::~ChangeUID()
{
}

int
ChangeUID::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpUnsigned, "UID", &_uid,
                  cpUnsigned, "time", &_timeout,
		  cpEnd) < 0)
    return -1;
  return 0;
}



int
ChangeUID::initialize(ErrorHandler *)
{
  // Schedule my timer
  _timer.initialize(this);
  _timer.schedule_after_ms(_timeout);

  return 0;
}


void
ChangeUID::timer_hook(Timer *, void *thunk)
{
  ChangeUID *me = (ChangeUID*) thunk;
  setuid(me->_uid);
  click_chatter("Setting UID to %d", me->_uid);
}
EXPORT_ELEMENT(ChangeUID)
