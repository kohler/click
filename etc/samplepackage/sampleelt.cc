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
// include your own config.h if appropriate
#include "sampleelt.hh"
#include <click/error.hh>
// note: <click/package.hh> no longer necessary

SamplePackageElement::SamplePackageElement()
{
  // CONSTRUCTOR MUST MOD_INC_USE_COUNT
  MOD_INC_USE_COUNT;
}

SamplePackageElement::~SamplePackageElement()
{
  // DESTRUCTOR MUST MOD_DEC_USE_COUNT
  MOD_DEC_USE_COUNT;
}

int
SamplePackageElement::initialize(ErrorHandler *errh)
{
  errh->message("Successfully linked with package!");
  return 0;
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
SamplePackageElement::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

EXPORT_ELEMENT(SamplePackageElement)
