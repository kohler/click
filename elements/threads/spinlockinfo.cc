/*
 * spinlockinfo.{cc,hh} -- element stores spinlocks
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "spinlockinfo.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/nameinfo.hh>
#include <click/error.hh>
CLICK_DECLS

SpinlockInfo::SpinlockInfo()
{
}

SpinlockInfo::~SpinlockInfo()
{
}

int
SpinlockInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (NameDB *ndb = NameInfo::getdb(NameInfo::T_SPINLOCK, this,
				      sizeof(Spinlock *), true)) {
	_spinlocks.reserve(conf.size());
	String name;
	for (int i = 0; i < conf.size(); ++i)
	    if (cp_string(conf[i], &name)) {
		_spinlocks.push_back(Spinlock());
		Spinlock *spinptr = &_spinlocks.back();
		ndb->define(name, &spinptr, sizeof(Spinlock *));
	    } else
		errh->error("bad NAME");
    } else
	errh->error("out of memory!");
    return errh->nerrors() ? -1 : 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SpinlockInfo)
