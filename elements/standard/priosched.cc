/*
 * priosched.{cc,hh} -- priority scheduler element
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

#include <click/config.h>
#include <click/package.hh>
#include "priosched.hh"

PrioSched::PrioSched()
{
  MOD_INC_USE_COUNT;
  add_output();
}

PrioSched::~PrioSched()
{
  MOD_DEC_USE_COUNT;
}

void
PrioSched::notify_ninputs(int i)
{
  set_ninputs(i);
}

Packet *
PrioSched::pull(int)
{
  for (int i = 0; i < ninputs(); i++) {
    Packet *p = input(i).pull();
    if (p)
      return p;
  }
  return 0;
}

EXPORT_ELEMENT(PrioSched)
ELEMENT_MT_SAFE(PrioSched)
