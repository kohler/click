/*
 * checkipheader.{cc,hh} -- element checks IP header for correctness
 * (checksums, lengths, source addresses)
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
#include "checkipheader.hh"
#include "click_ip.h"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/alignmentinfo.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

CheckIPHeader::CheckIPHeader()
  : _bad_src(0), _drops(0)
{
  add_input();
  add_output();
}

CheckIPHeader::~CheckIPHeader()
{
  delete[] _bad_src;
}

CheckIPHeader *
CheckIPHeader::clone() const
{
  return new CheckIPHeader();
}

void
CheckIPHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
CheckIPHeader::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() > 1)
    return errh->error("too many arguments to `CheckIPHeader([ADDRS])'");
  
  Vector<u_int> ips;
  ips.push_back(0);
  ips.push_back(0xffffffff);

  if (conf.size()) {
    Vector<String> words;
    u_int a;
    cp_spacevec(conf[0], words);
    for (int j = 0; j < words.size(); j++) {
      if (!cp_ip_address(words[j], (unsigned char *)&a, this))
	return errh->error("expects IPADDRESS");
      for (int j = 0; j < ips.size(); j++)
	if (ips[j] == a)
	  goto repeat;
      ips.push_back(a);
     repeat: ;
    }
  }

  delete[] _bad_src;
  _n_bad_src = ips.size();
  if (_n_bad_src) {
    _bad_src = new u_int [_n_bad_src];
    memcpy(_bad_src, &ips[0], sizeof(u_int) * ips.size());
  } else
    _bad_src = 0;

#ifdef __KERNEL__
  // check alignment
  {
    int ans, c, o;
    ans = AlignmentInfo::query(this, 0, c, o);
    _aligned = (ans && c == 4 && o == 0);
    if (!_aligned)
      errh->warning("IP header unaligned, cannot use fast IP checksum");
    if (!ans)
      errh->message("(Try passing the configuration through `click-align'.)");
  }
#endif
  
  return 0;
}

void
CheckIPHeader::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("IP checksum failed");
  _drops++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

Packet *
CheckIPHeader::simple_action(Packet *p)
{
  const click_ip *ip = reinterpret_cast<const click_ip *>(p->data());
  unsigned int src;
  unsigned hlen, len;
  
  if (p->length() < sizeof(click_ip))
    goto bad;
  
  if (ip->ip_v != 4)
    goto bad;
  
  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip))
    goto bad;
  
  len = ntohs(ip->ip_len);
  if (len > p->length() || len < hlen)
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

  // shorten packet according to IP length field -- 7/28/2000
  if (p->length() > len)
    p->take(p->length() - len);
  
  return(p);
  
 bad:
  drop_it(p);
  return 0;
}

static String
CheckIPHeader_read_drops(Element *xf, void *)
{
  CheckIPHeader *f = (CheckIPHeader *)xf;
  return String(f->drops()) + "\n";
}

void
CheckIPHeader::add_handlers()
{
  add_read_handler("drops", CheckIPHeader_read_drops, 0);
}

EXPORT_ELEMENT(CheckIPHeader)
