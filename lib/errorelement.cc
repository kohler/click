/*
 * errorelement.{cc,hh} -- an element that does absolutely nothing
 * Used as a placeholder for undefined element classes.
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
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

int
ErrorElement::configure(const String &, ErrorHandler *)
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

void
ErrorElement::add_handlers()
{
  /* no handlers whatsoever -- not even the defaults */
}

EXPORT_ELEMENT(ErrorElement)
