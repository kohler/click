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
UnlimitedElement::notify_inputs(int n)
{
  if (unlimited_inputs())
    add_inputs(n - ninputs());
}

void
UnlimitedElement::notify_outputs(int n)
{
  if (unlimited_outputs())
    add_outputs(n - noutputs());
}
