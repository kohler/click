/*
 * spew.{cc,hh} -- element is crap for benchmarks
 * Robert Morris
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "spew.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"
#include "elements/ethernet/arpquerier.hh"
#include "click_ether.h"
#include "click_ip.h"

Spew::Spew()
  : Element(0, 1), _timer(this)
{
  _done = 0;
  _n = -1;
  _quit = 1;
}

Spew *
Spew::clone() const
{
  return new Spew();
}

int
Spew::configure(const String &conf, ErrorHandler *errh)
{
  _quit = true;
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "number of packets to spew", &_n,
		     cpOptional,
		     cpBool, "quit after spewing", &_quit,
		     0);
}

int
Spew::initialize(ErrorHandler *errh)
{
  _done = 0;
  if (_n <= 0)
    errh->warning("will not spew any packets (%d <= 0)", _n);
  _timer.schedule_after_ms(0);
  return 0;
}

void
Spew::uninitialize()
{
  _timer.unschedule();
}

void
Spew::run_scheduled()
{
  spew_some();
  _timer.schedule_after_ms(0);
}

void
Spew::spew_some()
{
  if (_done >= _n)
    return;
  
#if !defined(__KERNEL__)
  Element *o = output(0).element();
  ARPQuerier *aq = (ARPQuerier *)o->is_a_cast("ARPQuerier");
  if(aq){
    click_chatter("Spew is hacking ARPQuerier");
    aq->insert(IPAddress("2.0.0.2"), EtherAddress((u_char*)"\x00\xa0\xc9\x9c\xfd\x9c"));
    aq->insert(IPAddress("18.26.4.97"), EtherAddress((u_char*)"\x00\x00\xc0\xca\x68\xef"));
  }
#endif
  
  Packet *p = Packet::make(14 + 20 + 16);
  struct ether_header *e = (struct ether_header *) p->data();
  struct ip *ip = (struct ip *) (e + 1);
  unsigned short sum;

  memcpy(e->ether_dhost, "\x00\x00\xc0\xae\x67\xef", 6); /* to cone */
  memcpy(e->ether_shost, "\x00\x80\xc8\x4b\x25\x00", 6); /* from darkstar */
  e->ether_type = htons(ETHERTYPE_IP);
  
  ip->ip_hl = 5;
  ip->ip_v = 4;
  ip->ip_tos = 0;
  ip->ip_len = htons(36);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 255;
  ip->ip_p = 17; /* udp */
  ip->ip_src.s_addr = htonl(0x01000001); /* from cone */
  ip->ip_dst.s_addr = htonl(0x02000002); /* to darkstar */
  ip->ip_sum = 0;
  sum = ip->ip_sum = in_cksum((unsigned char *)ip, 20);

  p->set_dst_ip_anno(IPAddress(p->data() + 30));
  
#if 1
  p->pull(14); // For CheckIPChecksum &c
#endif
  
  int max = _n - _done;

  for (int i = 0; i < max; i++){
#if 1
    ip->ip_ttl = 255; // Counteract DecTTL.
#endif
#if 1
    ip->ip_sum = sum; // Counteract DecTTL.
#endif

#if 1
    output(0).push(p);
#endif

#if 0
    p->spew_push(14); // Counteract Strip (p->push(14) is expensive).
#endif
#if 0
    p->pull(14); // Counteract ARPQuerier.
#endif
#if 0
    router()->run_scheduled(); // For Queue.
#endif
  }
  _done += max;
  if (_done >= _n && _quit)
    router()->please_stop_driver();
}

EXPORT_ELEMENT(Spew)
ELEMENT_REQUIRES(ip)
