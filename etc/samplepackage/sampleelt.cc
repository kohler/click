/*
 * sampleelt.{cc,hh} -- sample package element
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
// ALWAYS INCLUDE click/config.h
#include <click/config.h>
// ALWAYS INCLUDE click/package.hh
#include <click/package.hh>
#include "sampleelt.hh"
#include <click/error.hh>

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

EXPORT_ELEMENT(SamplePackageElement)
