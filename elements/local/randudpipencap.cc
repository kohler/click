/*
 * randomudpipencap.{cc,hh} -- randomly sends udp/ip packets
 * Benjie Chen, Eddie Kohler (original RoundRobinUDPIPEncap)
 * Thomer M. Gil (modified to RandomUDPIPEncap)
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
#include "click_ip.h"
#include "randudpipencap.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#include "elements/standard/alignmentinfo.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

RandomUDPIPEncap::RandomUDPIPEncap()
  : _addrs(0), _total_prob(0), _no_of_addresses(0)
{
  add_input();
  add_output();
}

RandomUDPIPEncap::~RandomUDPIPEncap()
{
  uninitialize();
}

RandomUDPIPEncap *
RandomUDPIPEncap::clone() const
{
  return new RandomUDPIPEncap;
}

int
RandomUDPIPEncap::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() == 0)
    return errh->error("too few arguments");

  _naddrs = conf.size();
  _addrs = new Addrs[_naddrs];
  if (!_addrs)
    return errh->error("out of memory");

  int before = errh->nerrors();
  for (unsigned i = 0; i < _naddrs; i++) {
    Vector<String> words;
    cp_spacevec(conf[i], words);
    if (words.size() == 5)
      words.push_back("1");
    int sport, dport;
    int prob;
    if (words.size() != 6
	|| !cp_ip_address(words[0], (unsigned char *)&_addrs[i].saddr)
	|| !cp_integer(words[1], &sport)
	|| !cp_ip_address(words[2], (unsigned char *)&_addrs[i].daddr)
	|| !cp_integer(words[3], &dport)
	|| !cp_integer(words[4], &prob)
	|| !cp_bool(words[5], &_addrs[i].cksum)
	|| sport < 0 || sport >= 0x10000 || dport < 0 || dport >= 0x10000)
      errh->error("argument %d should be `SADDR SPORT DADDR DPORT PROB [CHECKSUM?]'", i);
    else {
      _addrs[i].sport = sport;
      _addrs[i].dport = dport;
      _addrs[i].id = 0;
      _total_prob += prob;
      _randoms[_no_of_addresses].n = _total_prob;
      _randoms[_no_of_addresses].a = &(_addrs[i]);
      _no_of_addresses++;
    }
  }
  if (errh->nerrors() != before)
    return -1;

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

int
RandomUDPIPEncap::initialize(ErrorHandler *)
{
  return 0;
}

void
RandomUDPIPEncap::uninitialize()
{
  delete[] _addrs;
  _addrs = 0;
}

Packet *
RandomUDPIPEncap::simple_action(Packet *p)
{
  // pick right address
  // XXX: Could be faster; hash.
  int pos = random() % _total_prob;
  int lb = 0;
  Addrs *addr = 0;
  for(short i=0; i<_no_of_addresses; i++) {
    if(pos < _randoms[i].n && pos >= lb) {
      addr = &_addrs[i];
      break;
    }
    lb = _randoms[i].n;
  }

  // add to packet
  WritablePacket *q = p->push(sizeof(click_udp) + sizeof(click_ip));
  click_ip *ip = reinterpret_cast<click_ip *>(q->data());
  click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

  // set up IP header
  ip->ip_v = IPVERSION;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(addr->id++);
  ip->ip_p = IP_PROTO_UDP;
  ip->ip_src = addr->saddr;
  ip->ip_dst = addr->daddr;
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;

  ip->ip_sum = 0;
#ifdef __KERNEL__
  if (_aligned) {
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
  } else {
#endif
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));
#ifdef __KERNEL__
  }
#endif
  
  q->set_dst_ip_anno(IPAddress(addr->daddr));
  q->set_ip_header(ip, sizeof(click_ip));

  // set up UDP header
  udp->uh_sport = htons(addr->sport);
  udp->uh_dport = htons(addr->dport);
  unsigned short len = q->length() - sizeof(click_ip);
  udp->uh_ulen = htons(len);
  if (addr->cksum) {
    unsigned csum = ~in_cksum((unsigned char *)udp, len) & 0xFFFF;
#ifdef __KERNEL__
    udp->uh_sum = csum_tcpudp_magic(addr->saddr.s_addr, addr->daddr.s_addr,
				    len, IP_PROTO_UDP, csum);
#else
    unsigned short *words = (unsigned short *)&ip->ip_src;
    csum += words[0];
    csum += words[1];
    csum += words[2];
    csum += words[3];
    csum += htons(IP_PROTO_UDP);
    csum += htons(len);
    while (csum >> 16)
      csum = (csum & 0xFFFF) + (csum >> 16);
    udp->uh_sum = ~csum & 0xFFFF;
#endif
  } else
    udp->uh_sum = 0;
  
  return q;
}

EXPORT_ELEMENT(RandomUDPIPEncap)
