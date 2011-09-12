/*
 * extradecap.{cc,hh} -- decapsultates 802.11 packets
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
#include "extradecap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
CLICK_DECLS

ExtraDecap::ExtraDecap()
{
}

ExtraDecap::~ExtraDecap()
{
}

int
ExtraDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _debug = false;
    return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
ExtraDecap::simple_action(Packet *p)
{

  click_wifi_extra *ceh = (click_wifi_extra *) p->data();

  if (ceh->magic == WIFI_EXTRA_MAGIC) {
      memcpy(WIFI_EXTRA_ANNO(p), p->data(), sizeof(click_wifi_extra));
      p->pull(sizeof(click_wifi_extra));
  }

  return p;
}


enum {H_DEBUG};

static String
ExtraDecap_read_param(Element *e, void *thunk)
{
  ExtraDecap *td = (ExtraDecap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int
ExtraDecap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  ExtraDecap *f = (ExtraDecap *)e;
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
ExtraDecap::add_handlers()
{
  add_read_handler("debug", ExtraDecap_read_param, H_DEBUG);

  add_write_handler("debug", ExtraDecap_write_param, H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(ExtraDecap)
