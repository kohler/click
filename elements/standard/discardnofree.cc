#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "discardnofree.hh"

DiscardNoFree::DiscardNoFree()
  : Element(1, 0)
{
}

void
DiscardNoFree::push(int, Packet *)
{
  // Don't kill().
}

bool
DiscardNoFree::wants_packet_upstream() const
{
  return input_is_pull(0);
}

void
DiscardNoFree::run_scheduled()
{
  while (Packet *p = input(0).pull())
    /* Don't kill() */;
}

EXPORT_ELEMENT(DiscardNoFree)
