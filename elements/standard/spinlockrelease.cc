/*
 * spinlockrelease.{cc,hh} -- element releases spinlock
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include <click/config.h>
#include <click/router.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include "spinlockinfo.hh"
#include "spinlockrelease.hh"
CLICK_DECLS

int
SpinlockRelease::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String name;
  if (cp_va_parse(conf, this, errh, cpString, "name of spinlock", &name, 0)<0)
    return -1;
  for (int i = 0; i < router()->nelements(); i++) {
    if (SpinlockInfo *si = 
	(SpinlockInfo *)(router()->element(i)->cast("SpinlockInfo"))) {
      _lock = si->query(name, id());
      if (!_lock)
        return errh->error("cannot locate spinlock %s",name.cc());
      break;
    }
  }
  return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SpinlockRelease)
