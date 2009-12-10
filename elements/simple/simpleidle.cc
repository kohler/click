/*
 * simpleidle.{cc,hh} -- element sits there and kills packets sans notification
 * Eddie Kohler
 *
 * Copyright (c) 2009 Intel Corporation
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
#include "simpleidle.hh"
CLICK_DECLS

SimpleIdle::SimpleIdle()
{
}

SimpleIdle::~SimpleIdle()
{
}

void
SimpleIdle::push(int, Packet *p)
{
    p->kill();
}

Packet *
SimpleIdle::pull(int)
{
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SimpleIdle)
ELEMENT_MT_SAFE(SimpleIdle)
