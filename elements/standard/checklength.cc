/*
 * checklength.{cc,hh} -- element checks packet length
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
#include "checklength.hh"
#include <click/confparse.hh>
#include <click/error.hh>

CheckLength::CheckLength()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

CheckLength::~CheckLength()
{
  MOD_DEC_USE_COUNT;
}

void
CheckLength::notify_noutputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  set_noutputs(n < 2 ? 1 : 2);
}

int
CheckLength::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "max length", &_max,
		     0);
}

void
CheckLength::push(int, Packet *p)
{
  if (p->length() > _max)
    checked_output_push(1, p);
  else
    output(0).push(p);
}

Packet *
CheckLength::pull(int)
{
  Packet *p = input(0).pull();
  if (p && p->length() > _max) {
    checked_output_push(1, p);
    return 0;
  } else
    return p;
}

EXPORT_ELEMENT(CheckLength)
ELEMENT_MT_SAFE(CheckLength)
