/*
 * errorelement.{cc,hh} -- an element that does absolutely nothing
 * Used as a placeholder for undefined element classes.
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
#include <click/standard/errorelement.hh>
#include <click/bitvector.hh>

ErrorElement::ErrorElement()
{
  MOD_INC_USE_COUNT;
}

ErrorElement::~ErrorElement()
{
  MOD_DEC_USE_COUNT;
}

ErrorElement *
ErrorElement::clone() const
{
  return new ErrorElement;
}

void
ErrorElement::notify_ninputs(int i)
{
  set_ninputs(i);
}

void
ErrorElement::notify_noutputs(int i)
{
  set_noutputs(i);
}

int
ErrorElement::configure(const Vector<String> &, ErrorHandler *)
{
  /* ignore any configuration arguments */
  return 0;
}

int
ErrorElement::initialize(ErrorHandler *)
{
  /* always fail */
  return -1;
}

EXPORT_ELEMENT(ErrorElement)
ELEMENT_HEADER(<click/standard/errorelement.hh>)
