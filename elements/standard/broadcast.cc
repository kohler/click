#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "broadcast.hh"
#include "glue.hh"

Broadcast *
Broadcast::clone() const
{
  return new Broadcast;
}

void
Broadcast::push(int, Packet *p)
{
  int n = noutputs();
  for (int i = 0; i < n - 1; i++)
    output(i).push(p->clone());
  if (n > 0)
    output(n - 1).push(p);
  else
    p->kill();
}

EXPORT_ELEMENT(Broadcast)

