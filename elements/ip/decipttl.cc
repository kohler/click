#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "decipttl.hh"
#include "click_ip.h"
#include "glue.hh"

DecIPTTL::DecIPTTL()
  : _drops(0)
{
  add_input();
  add_output();
}

DecIPTTL::~DecIPTTL()
{
}

void
DecIPTTL::notify_outputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  n = (n >= 2 ? 2 : 1);
  add_outputs(n - noutputs());
}

void
DecIPTTL::processing_vector(Vector<int> &in_v, int in_offset,
			      Vector<int> &out_v, int out_offset) const
{
  in_v[in_offset+0] = out_v[out_offset+0] = AGNOSTIC;
  if (noutputs() == 2)
    out_v[out_offset+1] = PUSH;
}

DecIPTTL *
DecIPTTL::clone() const
{
  return new DecIPTTL();
}

inline Packet *
DecIPTTL::smaction(Packet *p)
{
  assert(p->length() >= sizeof(struct ip));
  
  struct ip *ip = (struct ip *)p->data();
  if (ip->ip_ttl <= 1) {
    _drops++;
    if (noutputs() == 2)
      output(1).push(p);
    else
      p->kill();
    return 0;
    
  } else {
    p = p->uniqueify();
    ip = (struct ip *)p->data();
    ip->ip_ttl--;
    
    // 19.Aug.1999 - incrementally update IP checksum as suggested by SOSP
    // reviewers, according to RFC1141, as updated by RFC1624.
    // new_sum = ~(~old_sum + ~old_halfword + new_halfword)
    //         = ~(~old_sum + ~old_halfword + (old_halfword - 0x0100))
    //         = ~(~old_sum + ~old_halfword + old_halfword + ~0x0100)
    //         = ~(~old_sum + ~0 + ~0x0100)
    //         = ~(~old_sum + 0xFEFF)
    unsigned long sum = (~ntohs(ip->ip_sum) & 0xFFFF) + 0xFEFF;
    ip->ip_sum = ~htons(sum + (sum >> 16));
    
    return p;
  }
}

void
DecIPTTL::push(int, Packet *p)
{
  if ((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
DecIPTTL::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    p = smaction(p);
  return p;
}

static String
DecIPTTL_read_drops(Element *xf, void *)
{
  DecIPTTL *f = (DecIPTTL *)xf;
  return String(f->drops()) + "\n";
}

void
DecIPTTL::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read("drops", DecIPTTL_read_drops, 0);
}

EXPORT_ELEMENT(DecIPTTL)
