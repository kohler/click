#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "discard.hh"
#include "error.hh"
#include "confparse.hh"

Discard::Discard()
  : Element(1, 0)
{
}

void
Discard::push(int, Packet *p)
{
  p->kill();
}

bool
Discard::wants_packet_upstream() const
{
  return input_is_pull(0);
}

void
Discard::run_scheduled()
{
  while (Packet *p = input(0).pull())
    p->kill();
}

EXPORT_ELEMENT(Discard)
