/*
 * spinlockacquire.{cc,hh} -- element acquires spinlock
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 * Copyright (c) 2008 Meraki, Inc.
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
#include <click/nameinfo.hh>
#include "spinlockacquire.hh"
CLICK_DECLS

int
SpinlockAcquire::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String name;
    if (Args(conf, this, errh).read_mp("LOCK", name).complete() < 0)
	return -1;
    if (!NameInfo::query(NameInfo::T_SPINLOCK, this, name, &_lock, sizeof(Spinlock *)))
	return errh->error("cannot locate spinlock %s", name.c_str());
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SpinlockAcquire)
