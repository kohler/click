/*
 * clickyinfo.{cc,hh} -- do-nothing element storing Clicky information
 * Eddie Kohler
 *
 * Copyright (c) 2011 Regents of the University of California
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
#include "clickyinfo.hh"
CLICK_DECLS

ClickyInfo::ClickyInfo()
{
}

int
ClickyInfo::configure(Vector<String> &, ErrorHandler *)
{
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ClickyInfo)
