/*
 * extraencap.{cc,hh} -- encapsultates 802.11 packets
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
#include "extraencap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
CLICK_DECLS

ExtraEncap::ExtraEncap()
{
}

ExtraEncap::~ExtraEncap()
{
}

int
ExtraEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _debug = false;
    return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
ExtraEncap::simple_action(Packet *p)
{

  WritablePacket *p_out = p->uniqueify();
  if (!p_out) {
    p->kill();
    return 0;
  }

  p_out = p_out->push(sizeof(click_wifi_extra));
  if (p_out) {
	  click_wifi_extra *eh = (click_wifi_extra *) p_out->data();

	  memset(eh, 0, sizeof(click_wifi_extra));
	  memcpy(p_out->data(), WIFI_EXTRA_ANNO(p_out), sizeof(click_wifi_extra));
	  eh->magic = WIFI_EXTRA_MAGIC;
  }
  return p_out;
}


enum {H_DEBUG};

static String
ExtraEncap_read_param(Element *e, void *thunk)
{
  ExtraEncap *td = (ExtraEncap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int
ExtraEncap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  ExtraEncap *f = (ExtraEncap *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!BoolArg().parse(s, debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
ExtraEncap::add_handlers()
{
  add_read_handler("debug", ExtraEncap_read_param, H_DEBUG);

  add_write_handler("debug", ExtraEncap_write_param, H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(ExtraEncap)
