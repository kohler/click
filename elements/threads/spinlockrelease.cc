/*
 * spinlockrelease.{cc,hh} -- element releases spinlock
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008 Meraki, Inc.
 * Copyright (c) 1999-2013 Eddie Kohler
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
#include <click/router.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/nameinfo.hh>
#include "spinlockrelease.hh"
CLICK_DECLS

int
SpinlockRelease::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String name;
    if (Args(conf, this, errh).read_mp("LOCK", name).complete() < 0)
	return -1;
    if (!NameInfo::query(NameInfo::T_SPINLOCK, this, name, &_lock, sizeof(Spinlock *)))
	return errh->error("cannot locate spinlock %s", name.c_str());
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SpinlockRelease)
