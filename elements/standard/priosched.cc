#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "priosched.hh"

PrioSched::PrioSched()
{
  add_output();
}

PrioSched::~PrioSched()
{
}

Packet *
PrioSched::pull(int)
{
  for (int i = 0; i < ninputs(); i++) {
    Packet *p = input(i).pull();
    if (p)
      return p;
  }
  return 0;
}

EXPORT_ELEMENT(PrioSched)
