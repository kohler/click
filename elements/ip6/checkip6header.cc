/*
 * checkip6header.{cc,hh} -- element checks IP6 header for correctness
 * (lengths, source addresses)
 * Robert Morris , Peilei Fan
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "checkip6header.hh"
#include <click/click_ip6.h>
#include <click/ip6address.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/standard/alignmentinfo.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif


CheckIP6Header::CheckIP6Header()
  : _bad_src(0), _drops(0)
{
  add_input();
  add_output();
}

CheckIP6Header::~CheckIP6Header()
{
  delete[] _bad_src;
}

CheckIP6Header *
CheckIP6Header::clone() const
{
  return new CheckIP6Header();
}

void
CheckIP6Header::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
CheckIP6Header::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  
  if (conf.size() > 1)
    return errh->error("too many arguments to `CheckIP6Header([ADDRS])'");
 
 Vector<String> ips; 
 ips.push_back("0::0"); //bad IP6 address "0::0"
 ips.push_back("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); //another bad IP6 address

  if (conf.size()) {
    Vector<String> words;
    cp_spacevec(conf[0], words);
    IP6Address a;
    for (int j = 0; j < words.size(); j++) {
      if (!cp_ip6_address(words[j], (unsigned char *)&a)) { 
	return errh->error("expects IP6ADDRESS -a ");
      }
      for (int j = 0; j < ips.size(); j++) {
	IP6Address b = IP6Address(ips[j]);
	if (b == a)
	  goto repeat;
      }
      ips.push_back(a.s());
     repeat: ;
    }
  }
  
  _n_bad_src = ips.size();
  _bad_src = new IP6Address [_n_bad_src];
  
  for (int i = 0; i<_n_bad_src; i++) {
    _bad_src[i]= IP6Address(ips[i]);
  }

  return 0;
}

void 
CheckIP6Header::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("IP checksum failed");
  _drops++;
  
  if (noutputs() == 2) {
      output(1).push(p);
      click_chatter("noutputs()=2");
  }
  else { 
      p->kill();
    click_chatter("killed");
  }

}

Packet *
CheckIP6Header::simple_action(Packet *p)
{
  const click_ip6 *ip = reinterpret_cast <const click_ip6 *>( p->data());
  struct IP6Address src;
  
  //check if the packet is smaller than ip6 header
  if(p->length() < sizeof(click_ip6))  {
    click_chatter("CheckIP6Header: packet length %d smaller than header 
length %d", p->length(), sizeof(click_ip6));
    goto bad;
  }
  
 //check version
  if(ip->ip6_v != 6) {
    goto bad;
  }

  //check if the PayloadLength field is valid
   if(ntohs(ip->ip6_plen) > (p->length()-40)){
     click_chatter("CheckIP6Header: payload length field in ip6 header  %d, 
                   is greater than the payload length %d",
                   ntohs(ip->ip6_plen),
                   p->length() - 40);
     goto bad;
   }
   

  /*
   * discard illegal source addresses.
   * Configuration string should have listed all subnet
   * broadcast addresses known to this router.
   */
   src=ip->ip6_src;
   for(int i = 0; i < _n_bad_src; i++) {  
     if(src == _bad_src[i]) {
       goto bad;
     }
   }

  /*
   * discard illegal destinations.
   * We will do this in the IP6 routing table.
   * 
   * 
   */

  p->set_ip6_header(ip);

  // shorten packet according to IP6 payload length field 
  if(ntohs(ip->ip6_plen) < (p->length()-40)) 
    p->take(p->length() - 40 - ip->ip6_plen); 

  return(p);
  
 bad:
  drop_it(p);
  return 0;
}


static String
CheckIP6Header_read_drops(Element *xf, void *)
{
  CheckIP6Header *f = (CheckIP6Header *)xf;
  return String(f->drops()) + "\n";
}

void
CheckIP6Header::add_handlers()
{
  add_read_handler("drops", CheckIP6Header_read_drops, 0);
}

EXPORT_ELEMENT(CheckIP6Header)
