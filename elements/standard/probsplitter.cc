/*
 * probsplitter.{cc,hh} -- split packets onto different ports.
 * Benjie Chen
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
#include "probsplitter.hh"
#include <click/error.hh>
#include <click/confparse.hh>

ProbSplitter::ProbSplitter()
  : Element(1, 2)
{
  MOD_INC_USE_COUNT;
}

ProbSplitter::~ProbSplitter()
{
  MOD_DEC_USE_COUNT;
}

int
ProbSplitter::configure(const Vector<String> &, ErrorHandler *errh)
{
  return errh->error("ProbSplitter has been deprecated; use `RandomSample(DROP P)' instead.");
}

EXPORT_ELEMENT(ProbSplitter)
ELEMENT_MT_SAFE(ProbSplitter)
