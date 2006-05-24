// -*- c-basic-offset: 4 -*-
/*
 * kerneltap.{cc,hh} -- element accesses network via /dev/tap device
 * Robert Morris, Douglas S. J. De Couto, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2006 Regents of the University of California
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
#include "kerneltap.hh"

CLICK_DECLS

KernelTap::KernelTap()
{
    _tap = true;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel KernelTun)
EXPORT_ELEMENT(KernelTap)
