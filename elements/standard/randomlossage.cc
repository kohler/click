/*
 * randomlossage.{cc,hh} -- element probabilistically drops packets
 * Eddie Kohler
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
#include "randomlossage.hh"
#include <click/confparse.hh>
#include <click/error.hh>

RandomLossage::RandomLossage()
    : Element(1, 1)
{
    MOD_INC_USE_COUNT;
}

RandomLossage::~RandomLossage()
{
    MOD_DEC_USE_COUNT;
}

int
RandomLossage::configure(Vector<String> &, ErrorHandler *errh)
{
  errh->error("RandomLossage has been deprecated; use `RandomSample(DROP P)' instead.");
  return -1;
}

EXPORT_ELEMENT(RandomLossage)
ELEMENT_MT_SAFE(RandomLossage)
