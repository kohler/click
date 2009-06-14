// -*- c-basic-offset: 4 -*-
/*
 * quicknotequeue.{cc,hh} -- queue element that notifies quickly
 * Eddie Kohler
 *
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
#include "quicknotequeue.hh"
CLICK_DECLS

QuickNoteQueue::QuickNoteQueue()
{
}

QuickNoteQueue::~QuickNoteQueue()
{
}

void *
QuickNoteQueue::cast(const char *n)
{
    if (strcmp(n, "QuickNoteQueue") == 0)
	return (QuickNoteQueue *)this;
    else
	return FullNoteQueue::cast(n);
}

Packet *
QuickNoteQueue::pull(int)
{
    // Code taken from SimpleQueue::deq.
    int h = _head, t = _tail, nh = next_i(h);

    if (h != t)
	return pull_success(h, t, nh);
    else
	return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FullNoteQueue)
EXPORT_ELEMENT(QuickNoteQueue)
