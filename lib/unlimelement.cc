/*
 * unlimelement.{cc,hh} -- subclass this to get a simpler interface for
 * unlimited numbers of input and/or output ports
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "unlimelement.hh"
#include "confparse.hh"

UnlimitedElement::UnlimitedElement()
{
}

UnlimitedElement::UnlimitedElement(int ninputs, int noutputs)
  : Element(ninputs, noutputs)
{
}

void
UnlimitedElement::notify_ninputs(int n)
{
  if (unlimited_inputs())
    set_ninputs(n);
}

void
UnlimitedElement::notify_noutputs(int n)
{
  if (unlimited_outputs())
    set_noutputs(n);
}
