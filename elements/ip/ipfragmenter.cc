#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipfragmenter.hh"
#include "click_ip.h"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

IPFragmenter::IPFragmenter()
  : _drops(0)
{
  _fragments = 0;
  _mtu = 0;
  add_input();
  add_output();
}

IPFragmenter::~IPFragmenter()
{
}

void
IPFragmenter::notify_outputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  n = (n >= 2 ? 2 : 1);
  add_outputs(n - noutputs());
}

IPFragmenter *
IPFragmenter::clone() const
{
  return new IPFragmenter();
}


int
IPFragmenter::configure(const String &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpUnsigned, "MTU", &_mtu,
		  0) < 0)
    return -1;
  return 0;
}

int
IPFragmenter::optcopy(struct ip *ip1, struct ip *ip2)
{
  int opts = (ip1->ip_hl << 2) - sizeof(struct ip);
  u_char *base1 = (u_char *) (ip1 + 1);
  int i1, optlen;
  int i2 = 0;
  u_char *base2 = (u_char *) (ip2 + 1);

  for(i1 = 0; i1 < opts; i1 += optlen){
    int opt = base1[i1];
    if(opt == IPOPT_EOL)
      break;
    if(opt == IPOPT_NOP){
      optlen = 1;
    } else {
      optlen = base1[i1+1];
    }

    if(opt & 0x80){
      /* copy it */
      if(ip2){
        memcpy(base2 + i2, base1 + i1, optlen);
      }
      i2 += optlen;
    }
  }

  for( ; i2 & 3; i2++)
    if(ip2)
      base2[i2] = IPOPT_EOL;

  return(i2);
}

inline Packet *
IPFragmenter::smaction(Packet *p)
{
  if (p->length() <= _mtu)
    return(p);

  struct ip *ip = (struct ip *)p->data();
  int hlen = ip->ip_hl << 2;
  int len = (_mtu - hlen) & ~7;
  int ipoff = ntohs(ip->ip_off);
  if((ipoff & IP_DF) || len < 8){
    click_chatter("IPFragmenter(%d) DF %s %s len=%d",
                  _mtu,
                  IPAddress(ip->ip_src).s().cc(),
                  IPAddress(ip->ip_dst).s().cc(),
                  p->length());
    _drops++;
    if (noutputs() == 2)
      output(1).push(p);
    else
      p->kill();
    return 0;
  }

  int olen = optcopy(ip, (struct ip *)0);
  int h1len = sizeof(*ip) + olen;
  int plen = ntohs(ip->ip_len);
  int off;
  for(off = hlen + len; off < plen; off += len){
    int p1datalen = plen - off;
    if(p1datalen > len)
      p1datalen = len;
    int p1len = p1datalen + h1len;
    Packet *p1 = Packet::make(p1len);
    struct ip *ip1 = (struct ip *) p1->data();

    *ip1 = *ip;
    optcopy(ip, ip1);

    assert(off + p1datalen <= p->length());
    memcpy(p1->data() + h1len, p->data() + off, p1datalen);
    
    ip1->ip_hl = h1len >> 2;
    ip1->ip_off = ((off - hlen) >> 3) + (ipoff & IP_OFFMASK);
    if(ipoff & IP_MF)
      ip1->ip_off |= IP_MF;
    if(off + p1datalen < plen)
      ip1->ip_off |= IP_MF;
    ip1->ip_off = htons(ip1->ip_off);
    ip1->ip_len = htons(p1len);
    ip1->ip_sum = 0;
    ip1->ip_sum = in_cksum(p1->data(), h1len);

    // XXX copy all annotations?
    p1->set_dst_ip_anno(p->dst_ip_anno());
    p1->set_mac_broadcast_anno(p->mac_broadcast_anno());

    output(0).push(p1);
    _fragments++;
  }

  p = p->take(p->length() - (hlen + len));
  ip = (struct ip *) p->data();
  ip->ip_len = htons(hlen + len);
  ip->ip_off = htons(ipoff | IP_MF);
  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *) ip, hlen);

  return(p);
}

static String
IPFragmenter_read_drops(Element *xf, void *)
{
  IPFragmenter *f = (IPFragmenter *)xf;
  return String(f->drops()) + "\n";
}

static String
IPFragmenter_read_fragments(Element *xf, void *)
{
  IPFragmenter *f = (IPFragmenter *)xf;
  return String(f->fragments()) + "\n";
}

void
IPFragmenter::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read("drops", IPFragmenter_read_drops, 0);
  fcr->add_read("fragments", IPFragmenter_read_fragments, 0);
}


void
IPFragmenter::push(int, Packet *p)
{
  if (p->length() <= _mtu)
    output(0).push(p);
  else if ((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
IPFragmenter::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    p = smaction(p);
  return(p);
}


EXPORT_ELEMENT(IPFragmenter)
