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
#include "elements/standard/alignmentinfo.hh"

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
IPInputCombo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "color", &_color,
		  cpIgnoreRest,
		  0) < 0)
    return -1;

  Vector<u_int> ips;
  ips.push_back(0);
  ips.push_back(0xffffffff);

  if (conf.size() > 1) {
    String s = conf[1];
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

#ifdef __KERNEL__
  // check alignment
  {
    int ans, c, o;
    ans = AlignmentInfo::query(this, 0, c, o);
    _aligned = (ans && c == 4 && o == 2);
    if (!_aligned)
      errh->warning("IP header unaligned, cannot use fast IP checksum");
    if (!ans)
      errh->message("(Try passing the configuration through `click-align'.)");
  }
#endif
  
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
  
#ifdef __KERNEL__
  if (_aligned) {
    if (ip_fast_csum((unsigned char *)ip, ip->ip_hl) != 0)
      goto bad;
  } else {
#endif
  if (in_cksum((unsigned char *)ip, hlen) != 0)
    goto bad;
#ifdef __KERNEL__
  }
#endif
  
  if(ntohs(ip->ip_len) < hlen)
    goto bad;

  /*
   * RFC1812 5.3.7 and 4.2.2.11: discard illegal source addresses.
   * Configuration string should have listed all subnet
   * broadcast addresses known to this router.
   */
  src = ip->ip_src.s_addr;
  for(int i = 0; i < _n_bad_src; i++)
    if(src == _bad_src[i])
      goto bad;

  /*
   * RFC1812 4.2.3.1: discard illegal destinations.
   * We now do this in the IP routing table.
   */

  p->set_ip_header(ip, hlen);
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
IPInputCombo::add_handlers()
{
  add_read_handler("drops", IPInputCombo_read_drops, 0);
}

EXPORT_ELEMENT(IPInputCombo)
