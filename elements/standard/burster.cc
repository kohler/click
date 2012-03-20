/*
 * burster.{cc,hh} -- element pulls packets from input, pushes to output
 * in bursts
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2009 Meraki, Inc.
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
#include "burster.hh"
CLICK_DECLS

Burster::Burster()
{
    _burst = 8;
}

void *
Burster::cast(const char *name)
{
    if (strcmp(name, "TimedUnqueue") == 0)
	return (TimedUnqueue *) this;
    else
	return Element::cast(name);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(TimedUnqueue)
EXPORT_ELEMENT(Burster)
