/*
 * priosched.{cc,hh} -- priority scheduler element
 * Robert Morris, Eddie Kohler
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
#include "priosched.hh"
CLICK_DECLS

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

CLICK_ENDDECLS
EXPORT_ELEMENT(PrioSched)
ELEMENT_MT_SAFE(PrioSched)
