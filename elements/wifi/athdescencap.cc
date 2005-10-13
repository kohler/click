/*
 * athdescencap.{cc,hh} -- encapsultates 802.11 packets
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
#include "athdescencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include "athdesc.h"
CLICK_DECLS


AthdescEncap::AthdescEncap()
{
}

AthdescEncap::~AthdescEncap()
{
}

int
AthdescEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
AthdescEncap::simple_action(Packet *p)
{

  WritablePacket *p_out = p->push(ATHDESC_HEADER_SIZE);
  if (!p_out) { return 0; }

  struct ar5212_desc *desc  = (struct ar5212_desc *) (p_out->data() + 8);
  click_wifi_extra *ceh = (click_wifi_extra *) p_out->all_user_anno();
  
  memset((void *)p_out->data(), 0, ATHDESC_HEADER_SIZE);
  
  desc->xmit_power = ceh->power;
  desc->xmit_rate0 = dot11_to_ratecode(ceh->rate);
  if (ceh->max_tries > 0) {
    desc->xmit_tries0 = ceh->max_tries - 1;
  }
  
  return p_out;
}


enum {H_DEBUG};

static String 
AthdescEncap_read_param(Element *e, void *thunk)
{
  AthdescEncap *td = (AthdescEncap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
AthdescEncap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AthdescEncap *f = (AthdescEncap *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
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
AthdescEncap::add_handlers()
{
	add_read_handler("debug", AthdescEncap_read_param, (void *) H_DEBUG);
	add_write_handler("debug", AthdescEncap_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(AthdescEncap)
