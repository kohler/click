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
ErrorElement::add_handlers(HandlerRegistry *)
{
  /* no handlers whatsoever -- not even the defaults */
}

EXPORT_ELEMENT(ErrorElement)
