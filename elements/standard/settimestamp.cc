// -*- c-basic-offset: 4 -*-
/*
 * settimestamp.{cc,hh} -- set timestamp annotations
 * Douglas S. J. De Couto
 * based on setperfcount.{cc,hh}
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "settimestamp.hh"
#include <click/confparse.hh>
CLICK_DECLS

SetTimestamp::SetTimestamp()
    : Element(1, 1)
{
}

SetTimestamp::~SetTimestamp()
{
}

int
SetTimestamp::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _tv._sec = -1;
    if (cp_va_parse(conf, this, errh,
		    cpOptional,
		    cpTimestamp, "timestamp", &_tv,
		    cpEnd) < 0)
	return -1;
    return 0;
}

Packet *
SetTimestamp::simple_action(Packet *p)
{
    Timestamp& tv = p->timestamp_anno();
    if (_tv._sec >= 0)
	tv = _tv;
    else
	tv.set_now();
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTimestamp)
