#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/args.hh>
#include <clicknet/udp.h>
#include <click/packet.hh>
#include "gridsrforwarder.hh"
CLICK_DECLS

GridSRForwarder::GridSRForwarder()
{

}

GridSRForwarder::~GridSRForwarder()
{

}

void *
GridSRForwarder::cast(const char *name)
{
  // XXX strcmp sucks
  if (strcmp(name, "GridSRForwarder") == 0)
    return (void *) this;
  else
    return 0;
}

int
GridSRForwarder::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("IP", _ip).complete();
}

int
GridSRForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
GridSRForwarder::push(int port, Packet *p)
{
  assert(port == 0);
  handle_host(p);
}

void
GridSRForwarder::handle_host(Packet *p)
{
  // expects ip packets

  if (p->length() != 124 - 14) {
    click_chatter("GridSRForwarder: bad SR packet length %d, should be 124 - 14, dropping",
		  p->length());
    p->kill();
    return ;
  }

  click_ip *iph = (click_ip *) p->data();
  click_udp *uh = (click_udp *) (iph + 1);
  unsigned short *sp = (unsigned short *) (uh + 1);

  short route_len = ntohs(*sp);
  sp++;
  Vector<unsigned short> hops;
  for (short i = 0; i < route_len; i++, sp++)
    hops.push_back(ntohs(*sp));

  unsigned long *lp = (unsigned long *) sp;
  unsigned long src = ntohl(*lp);
  lp++;

  char *cp = (char *) lp;
  char exp_id[18];
  memset(exp_id, 0, sizeof(exp_id));
  memcpy(exp_id, cp, sizeof(exp_id));
  exp_id[17] = 0;

  cp += 18;
  char route_id[18];
  memset(route_id, 0, sizeof(route_id));
  memcpy(route_id, cp, sizeof(route_id));
  route_id[17] = 0;

  cp += 18;
  lp = (unsigned long *) cp;
  unsigned long packet_count = ntohl(*lp);

  if (route_len < 1) {
    click_chatter("GridSRForwarder: no hop data, dropping");
    p->kill();
    return;
  }

  unsigned long my_addr = ntohl(_ip.addr()) & 0xff;
  if (my_addr != hops[0]) {
    click_chatter("GridSRForwarder: this hop not us!  dropping");
    p->kill();
    return;
  }

  if (route_len == 1) {
    // pkt ought to be for us, kick up to sr-receiver
    output(1).push(p);
  }
  else {
    // perform sr forwarding
    sp = (unsigned short *) (uh + 1);
    *sp = htons(route_len - 1);
    sp++;
    for (int i = 1; i < route_len; i++, sp++)
      *sp = htons(hops[i]);
    lp = (unsigned long *)sp;
    *lp = htonl(src);
    lp++;
    cp = (char *) lp;
    memcpy(cp, route_id, 18);
    cp += 18;
    memcpy(cp, exp_id, 18);
    cp += 18;
    lp = (unsigned long *) cp;
    *lp = htonl(packet_count);

    // get the ip addresses right...
    iph->ip_src = _ip;
    iph->ip_dst.s_addr = htonl((ntohl(_ip.addr()) & 0xffFFff00) | hops[1]);

    // rewrite udp checksum
    // what a mess...
    uh->uh_sum = 0;
    unsigned csum = ~click_in_cksum((unsigned char *) uh, ntohs(uh->uh_ulen)) & 0xFFFF;
    unsigned short *words = (unsigned short *) &iph->ip_src;
    csum += words[0];
    csum += words[1];
    csum += words[2];
    csum += words[3];
    csum += htons(IP_PROTO_UDP);
    csum += htons(p->length() - sizeof(click_ip));
    while (csum >> 16)
      csum = (csum & 0xFFFF) + (csum >> 16);
    uh->uh_sum = ~csum & 0xFFFF;

    output(0).push(p);
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GridSRForwarder)
ELEMENT_REQUIRES(userlevel)
