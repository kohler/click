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
