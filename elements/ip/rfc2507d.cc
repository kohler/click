/*
 * rfc2507c.{cc,hh} -- element compresses IP headers a la RFC 2507
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
#include "rfc2507d.hh"
#include <string.h>

RFC2507d::RFC2507d()
{
  add_input();
  add_output();
}

RFC2507d::~RFC2507d()
{
}

RFC2507d *
RFC2507d::clone() const
{
  return(new RFC2507d());
}

void
RFC2507d::decode(u_char * &in, unsigned short &x)
{
  x = ntohs(x);
  if(in[0] != 0){
    x += *in;
    in++;
  } else {
    in++;
    x += (*in << 8);
    in++;
    x += *in;
    in++;
  }
  x = htons(x);
}

void
RFC2507d::decode(u_char * &in, unsigned int &x)
{
  x = ntohl(x);
  if(in[0] != 0){
    x += *in;
    in++;
  } else {
    in++;
    x += (*in << 8);
    in++;
    x += *in;
    in++;
  }
  x = htonl(x);
}


Packet *
RFC2507d::simple_action(Packet *p)
{
  Packet *q = 0;

  if(p->length() < 2)
    goto out;

  if(p->data()[0] == PT_OTHER){
    q = Packet::make(p->length() - 1);
    memcpy(q->data(), p->data() + 1, p->length() - 1);
  } else if(p->data()[0] == PT_FULL_HEADER){
    click_chatter("2507d: got full header\n");
    int cid = p->data()[1] & 0xff;
    if(cid <= 0 || cid >= TCP_SPACE)
      goto out;
    struct tcpip *ctx = &_ccbs[cid]._context;
    memcpy(&(ctx->_ip), p->data() + 2, sizeof(click_ip));
    memcpy(&(ctx->_tcp), p->data() + 2 + sizeof(click_ip),
           sizeof(struct click_tcp));
    q = Packet::make(p->length() - 2);
    memcpy(q->data(), p->data() + 2, p->length() - 2);
  } else if(p->data()[0] == PT_COMPRESSED_TCP){
    int cid = p->data()[1] & 0xff;
    if(cid <= 0 || cid >= TCP_SPACE)
      goto out;

    struct tcpip *ctx = &_ccbs[cid]._context;
    int flags = p->data()[2];
    memcpy(&(ctx->_tcp.th_sum), p->data() + 3, 2);
    u_char *in = p->data() + 5;

    if(flags & (1 << 4)) /* P */
      ctx->_tcp.th_flags |= TH_PUSH;
    else
      ctx->_tcp.th_flags &= ~TH_PUSH;

    if(flags & 1){ /* U */
      ctx->_tcp.th_flags |= TH_URG;
      decode(in, ctx->_tcp.th_urp);
    } else {
      ctx->_tcp.th_flags &= ~TH_URG;
    }

    if(flags & (1 << 1)){ /* W */
      decode(in, ctx->_tcp.th_win);
    }

    if(flags & (1 << 2)){ /* A */
      decode(in, ctx->_tcp.th_ack);
    }

    if(flags & (1 << 3)){ /* S */
      decode(in, ctx->_tcp.th_seq);
    }

    if(flags & (1 << 5)){ /* I */
      decode(in, ctx->_ip.ip_id);
    } else {
      ctx->_ip.ip_id = htons(ntohs(ctx->_ip.ip_id) + 1);
    }

    int len = p->length() - (in - p->data());
    len += sizeof(click_ip) + sizeof(struct click_tcp);
    ctx->_ip.ip_len = htons(len);

    ctx->_ip.ip_sum = 0;
    ctx->_ip.ip_sum = in_cksum((unsigned char *) &(ctx->_ip), sizeof(click_ip));

    q = Packet::make(len);
    memcpy(q->data(), &(ctx->_ip), sizeof(click_ip));
    memcpy(q->data() + sizeof(click_ip),
           &(ctx->_tcp),
           sizeof(struct click_tcp));
    memcpy(q->data() + sizeof(click_ip) + sizeof(struct click_tcp),
           in,
           p->length() - (in - p->data()));
  } else {
    goto out;
  }

 out:
  if(q){
    click_ip iph;
    struct click_tcp tcph;
    memcpy(&iph, q->data(), sizeof(iph));
    memcpy(&tcph, q->data() + sizeof(click_ip), sizeof(tcph));
    click_chatter("seq %d len %d",
                (int)ntohl(tcph.th_seq),
                q->length() - sizeof(tcph) - sizeof(iph));
    
    {
      char *p = new char[q->length()];
      memcpy(p, q->data(), q->length());

      // check IP checksum
      click_ip *ipp = (click_ip *) p;
      int hlen = ipp->ip_hl << 2;
      if(in_cksum((unsigned char *)ipp, hlen) != 0){
        click_chatter(" ip cksum failed");
      }

      // check TCP checksum; include pseudoheader
      int len = ntohs(ipp->ip_len);
      // zero ip_v, ip_hl, ip_tos, ip_len, ip_off, ip_ttl
      memset(ipp, '\0', 9);
      ipp->ip_sum = htons(len - sizeof(click_ip));
      if(in_cksum((unsigned char *)p, len) != 0){
        click_chatter(" tcp cksum failed");
      }
      
      delete [] p;
    }
  }
  if(q == 0)
    click_chatter("RFC2507d: no q");
  p->kill();
  return(q);
}

EXPORT_ELEMENT(RFC2507d)
