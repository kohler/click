#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "arpresponder.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

ARPResponder::ARPResponder()
{
  add_input();
  add_output();
}

ARPResponder::~ARPResponder()
{
}

ARPResponder *
ARPResponder::clone() const
{
  return new ARPResponder;
}

int
ARPResponder::configure(const String &conf, ErrorHandler *errh)
{
  IPAddress ipa, mask;
  EtherAddress ena;
  
  Vector<String> args;
  cp_argvec(conf, args);

  _v.clear();
  
  for (int i = 0; i < args.size(); i++) {
    String arg = args[i];
    if (cp_ip_address(arg, ipa, &arg)
	&& cp_eat_space(arg)
        && cp_ip_address(arg, mask, &arg)
        && cp_eat_space(arg)
	&& cp_ethernet_address(arg, ena))
      set_map(ipa, mask, ena);
    else {
      errh->error("ARPResponder configuration expected ip, mask, and ether addr");
      return -1;
    }
  }
  
  return 0;
}

void
ARPResponder::set_map(IPAddress ipa, IPAddress mask, EtherAddress ena)
{
  struct Entry e;

  e._dst = ipa;
  e._mask = mask;
  e._ena = ena;
  _v.push_back(e);
}

Packet *
ARPResponder::make_response(u_char tha[6], /* him */
                            u_char tpa[4],
                            u_char sha[6], /* me */
                            u_char spa[4])
{
  struct ether_header *e;
  struct ether_arp *ea;
  Packet *q = Packet::make(sizeof(*e) + sizeof(*ea));
  memset(q->data(), '\0', q->length());
  e = (struct ether_header *) q->data();
  ea = (struct ether_arp *) (e + 1);
  memcpy(e->ether_dhost, tha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_ARP);
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
  ea->ea_hdr.ar_hln = 6;
  ea->ea_hdr.ar_pln = 4;
  ea->ea_hdr.ar_op = htons(ARPOP_REPLY);
  memcpy(ea->arp_tha, tha, 6);
  memcpy(ea->arp_tpa, tpa, 4);
  memcpy(ea->arp_sha, sha, 6);
  memcpy(ea->arp_spa, spa, 4);
  return q;
}

bool
ARPResponder::lookup(IPAddress a, EtherAddress &ena)
{
  int i, besti = -1;

  for(i = 0; i < _v.size(); i++){
    if((a.s_addr() & _v[i]._mask.s_addr()) == _v[i]._dst.s_addr()){
      if(besti == -1 || ~_v[i]._mask.s_addr() < ~_v[besti]._mask.s_addr()){
        besti = i;
      }
    }
  }

  if(besti == -1){
    return(false);
  } else {
    ena = _v[besti]._ena;
    return(true);
  }
}

Packet *
ARPResponder::simple_action(Packet *p)
{
  struct ether_header *e = (struct ether_header *) p->data();
  struct ether_arp *ea = (struct ether_arp *) (e + 1);
  unsigned int tpa;
  memcpy(&tpa, ea->arp_tpa, 4);
  IPAddress ipa = IPAddress(tpa);
  
  Packet *q = 0;
  if (p->length() >= sizeof(*e) + sizeof(struct ether_arp) &&
      ntohs(e->ether_type) == ETHERTYPE_ARP &&
      ntohs(ea->ea_hdr.ar_hrd) == ARPHRD_ETHER &&
      ntohs(ea->ea_hdr.ar_pro) == ETHERTYPE_IP &&
      ntohs(ea->ea_hdr.ar_op) == ARPOP_REQUEST) {
    EtherAddress ena;
    if(lookup(ipa, ena)){
      q = make_response(ea->arp_sha, ea->arp_spa,
			ena.data(), ea->arp_tpa);
    }
  } else {
    struct in_addr ina;
    memcpy(&ina, &ea->arp_tpa, 4);
  }
  
  p->kill();
  return(q);
}

EXPORT_ELEMENT(ARPResponder)


// generate Vector template instance
#include "vector.cc"
template class Vector<ARPResponder::Entry>;
