/*
 * lookupiproute2.{cc,hh} -- element looks up next-hop address in pokeable
 * routing table.
 * Thomer M. Gil
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
#include <click/config.h>
#include <click/package.hh>
#include "lookupiproute2.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

LookupIPRoute2::LookupIPRoute2()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

LookupIPRoute2::~LookupIPRoute2()
{
  MOD_DEC_USE_COUNT;
}

LookupIPRoute2 *
LookupIPRoute2::clone() const
{
  return new LookupIPRoute2;
}

void
LookupIPRoute2::push(int, Packet *p)
{
  unsigned gw = 0;
  int index = 0;

  IPAddress a = p->dst_ip_anno();

  /*
  add_route_handler("1.0.0.0/255.255.0.0 5.5.5.5", this, (void *)0, (ErrorHandler *)0);
  add_route_handler("2.1.0.0/255.255.0.0 1.1.1.1", this, (void *)0, (ErrorHandler *)0);
  add_route_handler("2.2.0.0/255.255.0.0 2.2.2.2", this, (void *)0, (ErrorHandler *)0);
  add_route_handler("2.244.0.0/255.255.0.0 3.3.3.3", this, (void *)0, (ErrorHandler *)0);
  add_route_handler("2.0.0.0/255.255.0.0 4.4.4.4", this, (void *)0, (ErrorHandler *)0);
  del_route_handler("2.1.3.0/255.255.0.0", this, (void *)0, (ErrorHandler *)0);
  click_chatter("Lookup for %x", ntohl(a.addr()));
  */

  if(_t.lookup(a.addr(), gw, index)) {
    p->set_dst_ip_anno(gw);
    // click_chatter("Gateway for %x is %x", ntohl(a.addr()), ntohl(gw));
  } else {
    click_chatter("No route found.");
  }
  output(0).push(p);
}

// Adds a route if not exists yet.
int
LookupIPRoute2::add_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);

  LookupIPRoute2* me = (LookupIPRoute2 *) e;

  for (int i = 0; i < args.size(); i++) {
    Vector<String> words;
    cp_spacevec(args[i], words);
    unsigned int dst, mask, gw;
    if (words.size() == 2
	&& cp_ip_prefix(words[0], (unsigned char *)&dst, (unsigned char *)&mask, true, me) // allow bare IP addresses
        && cp_ip_address(words[1], (unsigned char *)&gw, me))
      me->_t.add(dst, mask, gw);
    else {
      errh->error("expects DST/MASK GW");
      return -1;
    }
  }

  return 0;
}

// Deletes a route. Nothing happens when entry does not exist.
int
LookupIPRoute2::del_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);

  LookupIPRoute2* me = (LookupIPRoute2 *) e;

  for (int i = 0; i < args.size(); i++) {
    unsigned int dst, mask;
    if (cp_ip_prefix(args[i], (unsigned char *)&dst, (unsigned char *)&mask, true, me))
      me->_t.del(dst, mask);
    else {
      errh->error("expects DST/MASK");
      return -1;
    }
  }

  return 0;
}

// Prints the routing table.
String
LookupIPRoute2::look_route_handler(Element *e, void *)
{
  String ret;
  unsigned dst, mask, gw;
  LookupIPRoute2 *me;

  me = (LookupIPRoute2*) e;

  int size = me->_t.size();
  ret = "Entries: " + String(size) + "\nDST/MASK\tGW\n";
  if(size == 0)
    return ret;

  int seen = 0; // # of valid entries handled
  for(int i = 0; seen < size; i++) {
    if(me->_t.get(i, dst, mask, gw)) {  // false if not valid
      ret += IPAddress(dst).s() + "/" + \
             IPAddress(mask).s()+ "\t" + \
             IPAddress(gw).s()  + "\n";
      seen++;
    }
  }

  return ret;
}


void
LookupIPRoute2::add_handlers()
{
  add_write_handler("add", add_route_handler, 0);
  add_write_handler("del", del_route_handler, 0);
  add_read_handler("look", look_route_handler, 0);
}

EXPORT_ELEMENT(LookupIPRoute2)
