#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "elemfilter.hh"

void
ElementFilter::filter(Vector<Element *> &v)
{
  Vector<Element *> nv;
  for (int i = 0; i < v.size(); i++)
    if (match(v[i]))
      nv.push_back(v[i]);
  v = nv;
}


IsaElementFilter::IsaElementFilter(const String &isa_what)
  : _isa_what(isa_what)
{
}

bool
IsaElementFilter::match(Element *f)
{
  return f->is_a(_isa_what);
}


bool
WantsPacketUpstreamElementFilter::match(Element *f)
{
  return f->wants_packet_upstream();
}
