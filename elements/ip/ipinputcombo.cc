/*
 * ipinputcombo.{cc,hh} -- IP router input combination element
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipinputcombo.hh"
#include "click_ip.h"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"

IPInputCombo::IPInputCombo()
  : _drops(0), _bad_src(0)
{
  add_input();
  add_output();
}

IPInputCombo::~IPInputCombo()
{
  delete[] _bad_src;
}

IPInputCombo *
IPInputCombo::clone() const
{
  return new IPInputCombo();
}

int
IPInputCombo::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);

  if(cp_va_parse(args[0], this, errh,
                 cpUnsigned, "color", &_color,
                 0) < 0)
    return(-1);

  Vector<u_int> ips;
  ips.push_back(0);
  ips.push_back(0xffffffff);

  if (args.size() > 1) {
    String s = args[1];
    while (s) {
      u_int a;
      if (!cp_ip_address(s, (unsigned char *)&a, &s))
	return errh->error("expects IPADDRESS");
      cp_eat_space(s);
      for (int j = 0; j < ips.size(); j++)
	if (ips[j] == a)
	  goto repeat;
      ips.push_back(a);
     repeat: ;
    }
  }

  _n_bad_src = ips.size();
  _bad_src = new u_int [_n_bad_src];
  memcpy(_bad_src, &ips[0], sizeof(u_int) * ips.size());

  return 0;
}

inline Packet *
IPInputCombo::smaction(Packet *p)
{
  unsigned int src;

  /* Paint */
  p->set_color_anno(_color);

  /* Strip(14) */
  p->pull(14);

  /* GetIPAddress(16) */
  p->set_dst_ip_anno(IPAddress(p->data() + 16));

  /* CheckIPHeader */
  click_ip *ip = (click_ip *) p->data();
  unsigned hlen;
  
  if(p->length() < sizeof(click_ip))
    goto bad;
  
  if(ip->ip_v != 4)
    goto bad;
  
  hlen = ip->ip_hl << 2;
  if(hlen < sizeof(click_ip))
    goto bad;
  
  if(hlen > p->length())
    goto bad;
  
  if(in_cksum((unsigned char *)ip, hlen) != 0)
    goto bad;
  
  if(ntohs(ip->ip_len) < hlen)
    goto bad;

  src = ip->ip_src.s_addr;
  for(int i = 0; i < _n_bad_src; i++)
    if(src == _bad_src[i])
      goto bad;

  p->set_ip_header(ip);
  return(p);
  
 bad:
  if(_drops == 0)
    click_chatter("IP checksum failed");
  p->kill();
  _drops++;
  return(0);
}

void
IPInputCombo::push(int, Packet *p)
{
  if((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
IPInputCombo::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    p = smaction(p);
  return(p);
}

static String
IPInputCombo_read_drops(Element *xf, void *)
{
  IPInputCombo *f = (IPInputCombo *)xf;
  return String(f->drops()) + "\n";
}

void
IPInputCombo::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("drops", IPInputCombo_read_drops, 0);
}

EXPORT_ELEMENT(IPInputCombo)
