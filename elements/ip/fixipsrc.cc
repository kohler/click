/*
 * fixipsrc.{cc,hh} -- element sets IP source if Fix IP Source annotation on
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
#include "fixipsrc.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "click_ip.h"

FixIPSrc::FixIPSrc()
{
  add_input();
  add_output();
}

FixIPSrc::~FixIPSrc()
{
}

FixIPSrc *
FixIPSrc::clone() const
{
  return new FixIPSrc();
}

int
FixIPSrc::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  IPAddress a;

  if (cp_va_parse(conf, this, errh,
                  cpIPAddress, "local addr", &a,
		  0) < 0)
    return -1;
  _my_ip = a.in_addr();
  return 0;
}

void
FixIPSrc::fix_it(Packet *p)
{
  click_ip *ip = p->ip_header();
  p->set_fix_ip_src_anno(0);
  click_chatter("FixIPSrc changed %x to %x",
                ip->ip_src.s_addr,
                _my_ip.s_addr);
  ip->ip_src = _my_ip;
  int hlen = ip->ip_hl << 2;
  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, hlen);
}

Packet *
FixIPSrc::simple_action(Packet *p)
{
  click_ip *ip = p->ip_header();
  if(p->fix_ip_src_anno() && ip){
    fix_it(p);
  }
  return(p);
}

EXPORT_ELEMENT(FixIPSrc)
