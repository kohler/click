#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "annotate.hh"
#include "click_ip.h"
#include "confparse.hh"
#include "error.hh"

Annotate::Annotate()
  : Element(1, 1)
{
}

Packet *
Annotate::simple_action(Packet *p)
{
  struct ip *ip = (struct ip *) p->data();
  p->set_ip_tos_anno(ip->ip_tos);
  p->set_ip_ttl_anno(ip->ip_ttl);
  p->set_ip_off_anno(ip->ip_off);
  return p;
}

Annotate *
Annotate::clone() const
{
  return new Annotate();
}

EXPORT_ELEMENT(Annotate)
