/*
 * nullelement.{cc,hh} -- do-nothing element
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
#include "nullelement.hh"

NullElement::NullElement()
  : Element(1, 1)
{
}

NullElement *
NullElement::clone() const
{
  return new NullElement;
}

Packet *
NullElement::simple_action(Packet *p)
{
  return p;
}

EXPORT_ELEMENT(NullElement)
