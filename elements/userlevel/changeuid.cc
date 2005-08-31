// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * changeuid.{cc,hh} -- element relinquishes root privilege
 * Eddie Kohler
 *
 * Copyright (c) 2005 Regents of the University of California
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
#include "changeuid.hh"
#include <click/error.hh>
#include <unistd.h>
CLICK_DECLS

ChangeUID::ChangeUID()
{
}

ChangeUID::~ChangeUID()
{
}

int
ChangeUID::initialize(ErrorHandler *errh)
{
    if (setgid(getgid()) != 0 || setuid(getuid()) != 0)
	return errh->error("could not drop privilege: %s", strerror(errno));
    return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ChangeUID)
