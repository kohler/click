/*
 * errorelement.{cc,hh} -- an element that does absolutely nothing
 * Used as a placeholder for undefined element classes.
 * Eddie Kohler
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "errorelement.hh"
#include "bitvector.hh"

ErrorElement::ErrorElement()
{
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

Bitvector
ErrorElement::forward_flow(int) const
{
  /* packets don't flow anywhere (minimize spurious errors) */
  return Bitvector(noutputs(), false);
}

Bitvector
ErrorElement::backward_flow(int) const
{
  return Bitvector(ninputs(), false);
}

EXPORT_ELEMENT(ErrorElement)
