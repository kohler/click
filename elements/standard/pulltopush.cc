#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "pulltopush.hh"

PullToPush::PullToPush()
  : Element(1, 1)
{
}

bool
PullToPush::wants_packet_upstream() const
{
  return true;
}

void
PullToPush::run_scheduled()
{
  while (Packet *p = input(0).pull())
    output(0).push(p);
}

EXPORT_ELEMENT(PullToPush)
