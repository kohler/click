/*
 * sampleelt.{cc,hh} -- sample package element
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

// ALWAYS INCLUDE <click/config.h> FIRST
#include <click/config.h>

#include "sampleelt.hh"
#include <click/error.hh>
CLICK_DECLS

SamplePackageElement::SamplePackageElement()
{
}

SamplePackageElement::~SamplePackageElement()
{
}

int
SamplePackageElement::initialize(ErrorHandler *errh)
{
    errh->message("Successfully linked with package!");
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SamplePackageElement)
