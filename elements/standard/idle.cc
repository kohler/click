/*
 * idle.{cc,hh} -- element just sits there and kills packets
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
#include "idle.hh"

Idle::Idle() 
{
  MOD_INC_USE_COUNT;
}

Idle::~Idle()
{
  MOD_DEC_USE_COUNT;
}

void *
Idle::cast(const char *name)
{
  if (strcmp(name, "AbstractNotifier") == 0)
    return static_cast<AbstractNotifier *>(this);
  else if (strcmp(name, "Idle") == 0)
    return this;
  else
    return Element::cast(name);
}

void
Idle::notify_ninputs(int n)
{
  set_ninputs(n);
}

void
Idle::notify_noutputs(int n)
{
  set_noutputs(n);
}

void
Idle::push(int, Packet *p)
{
  p->kill();
}

Packet *
Idle::pull(int)
{
  return 0;
}

EXPORT_ELEMENT(Idle)
ELEMENT_MT_SAFE(Idle)
