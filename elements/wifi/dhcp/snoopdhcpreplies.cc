/*
 * snoopdhcpreplies.{cc,hh} -- decapsultates 802.11 packets
 * John Bicket
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "snoopdhcpreplies.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/dhcp.h>

CLICK_DECLS

SnoopDHCPReplies::SnoopDHCPReplies()
{
}

SnoopDHCPReplies::~SnoopDHCPReplies()
{
}

int
SnoopDHCPReplies::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _ifname = "";
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEVICE", cpString, "interface name", &_ifname, 
		  "DEBUG", cpBool, "Debug", &_debug,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
SnoopDHCPReplies::simple_action(Packet *p)
{
	click_dhcp *d = (click_dhcp *) (((char *)p->ip_header()) + sizeof(click_ip) + sizeof(click_udp));
	
	if (d->op == DHCP_BOOTREPLY) {
		IPAddress dst = d->yiaddr;
		click_chatter("%{element} got %d for %s\n", this, d->op, dst.s().c_str());
		/* add route via ifname */
		char tmp[512];
		sprintf(tmp, "/sbin/route del -host %s", dst.s().c_str());
		system(tmp);
		sprintf(tmp, "/sbin/route add -host %s/32 %s", dst.s().c_str(), _ifname.c_str());
		system(tmp);
	}
	return p;
}


enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  SnoopDHCPReplies *td = (SnoopDHCPReplies *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SnoopDHCPReplies *f = (SnoopDHCPReplies *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}
 
void
SnoopDHCPReplies::add_handlers()
{
	add_read_handler("debug", read_param, (void *) H_DEBUG);
	add_write_handler("debug", write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(SnoopDHCPReplies)
ELEMENT_REQUIRES(userlevel linux)
