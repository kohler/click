/*
 * checkip6header.{cc,hh} -- element checks IP6 header for correctness
 * (lengths, source addresses)
 * Robert Morris , Peilei Fan
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
#include "checkip6header.hh"
#include "click_ip6.h"
#include "ip6address.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/alignmentinfo.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif
//#include "bitvector.hh"

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
  click_chatter("***************configure 1*************"); 
 
 Vector<String> ips; 
 ips.push_back("0::0"); //bad IP6 address "0::0"
 ips.push_back("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); //another bad IP6 address

  if (conf.size()) {
    String s = conf[0];
    IP6Address a;
    while (s) {
      errh->error(s);
      if (!cp_ip6_address(s, (unsigned char *)&a, &s))
	{ errh->error(a.s());
	  return errh->error("expects IP6ADDRESS -a ");}
      click_chatter(a.s());
      cp_eat_space(s);
      for (int j = 0; j < ips.size(); j++) {
	IP6Address b= IP6Address(ips[j]);
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

//  if (conf.size()) {
//      String s = conf[0];
//      IP6Address a;    
//      int i=0;
//      while (s) {
//         if (!cp_ip6_address(s, (unsigned char *)&a, &s))
//  	return errh->error("expects IP6ADDRESS -b");
//        cp_eat_space(s);
//        for (int j = 0; j < i; j++)
//  	if (_bad_src[j] == a)
//  	  goto repeat2;
//        _bad_src[i]=a;
//        i++;
//      repeat2: ;
//      } 
// }

 click_chatter("\n ########## CheckIP6Header conf successful ! \n"); 
 return 0;
}

void 
CheckIP6Header::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("IP checksum failed");
  _drops++;
  
  if (noutputs() == 2)
    {
      output(1).push(p);
      click_chatter("noutputs()=2");
    }
    
  else
    { 
      p->kill();
    click_chatter("killed");
    }
}

Packet *
CheckIP6Header::simple_action(Packet *p)
{
  //click_chatter("CheckIP6Header::smaction ");
  const click_ip6 *ip = (const click_ip6 *) p->data();
 
  
  const click_ip6 *ip66;
  struct IP6Address src;
  //unsigned hlen;
  
//check if the packet is bigger than ip6 header
  click_chatter("\n length: %x \n", p->length());
  if(p->length() < sizeof(click_ip6))        
    {
      click_chatter(" length is not right");
    goto bad;
    }
  
//check version
  click_chatter("\n version: %x \n", ip->ip6_v);
  if(ip->ip6_v != 6)
    {
    click_chatter("NOT VERSION 6");
    goto bad;
    }

//check if the PayloadLength field is valid
   click_chatter("\n payload length: %x \n", ip->ip6_plen);
   if(ntohs(ip->ip6_plen) < (p->length()-40))
    goto bad;

  
 
  /*
   * RFC1812 5.3.7 and 4.2.2.11: discard illegal source addresses.
   * Configuration string should have listed all subnet
   * broadcast addresses known to this router.
   */
   // src = ip->ip6_src.s_addr;
   
   src=ip->ip6_src;
   //click_chatter("\n source of the packet: ");
   //(ip->ip6_src).print();
   for(int i = 0; i < _n_bad_src; i++)
     {
       //click_chatter("\n************bad src %d \n", i);
       //_bad_src[i].print();
     if(src == _bad_src[i])
       {
	 click_chatter("\n***************bad src found*********************\n");
       goto bad;
       }
     }
  /*
   * RFC1812 4.2.3.1: discard illegal destinations.
   * We now do this in the IP routing table.
   */
   //click_chatter("before setip6 header");
  p->set_ip6_header(ip);
  click_chatter("\n ######### In CheckIP6Header: set ip6 header \n");
  //p = p->uniqueify();
  ip66 = p->ip6_header();
  //(ip66->ip6_src).print();
  //(ip66->ip6_dst).print();
  click_chatter(" \n hop limit is : %x \n", ip66->ip6_hlim);
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
