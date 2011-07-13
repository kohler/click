// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * tohostsniffers.{cc,hh} -- element sends packets to sniffers
 * Eddie Kohler; based on tohost.cc
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001 International Computer Science Institute
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
#include "tohostsniffers.hh"
CLICK_DECLS

ToHostSniffers::ToHostSniffers()
{
    // other stuff belongs to ToHost
    _sniffers = true;
}

ToHostSniffers::~ToHostSniffers()
{
    // other stuff belongs to ToHost
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ToHost bsdmodule)
EXPORT_ELEMENT(ToHostSniffers)
