#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "setipchecksum.hh"
#include "glue.hh"
#include "click_ip.h"

SetIPChecksum::SetIPChecksum()
{
  add_input();
  add_output();
}

SetIPChecksum::~SetIPChecksum()
{
}

SetIPChecksum *
SetIPChecksum::clone() const
{
  return new SetIPChecksum();
}

Packet *
SetIPChecksum::simple_action(Packet *p)
{
  struct ip *ip = (struct ip *) p->data();
  int hlen;

  if(p->length() < sizeof(struct ip))
    goto bad;

  hlen = ip->ip_hl << 2;
  if(hlen < (int)sizeof(struct ip) || hlen > (int)p->length())
    goto bad;

  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, hlen);
  
  return(p);

 bad:
  click_chatter("SetIPChecksum: bad lengths");
  p->kill();
  return(0);
}

EXPORT_ELEMENT(SetIPChecksum)
