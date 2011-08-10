/*
 * ethercount.{cc,hh} -- sets wifi txrate annotation on a packet
 * John Bicket
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include "ethercount.hh"
CLICK_DECLS

EtherCount::EtherCount()
{
}

EtherCount::~EtherCount()
{
}

Packet *
EtherCount::simple_action(Packet *p_in)
{
	click_ether *eh = (click_ether *) p_in->data();
	EtherAddress src = EtherAddress(eh->ether_shost);
	DstInfo *nfo = _neighbors.findp(src);
	if (!nfo) {
		DstInfo foo = DstInfo(src);
		_neighbors.insert(src, foo);
		nfo = _neighbors.findp(src);
	}
	nfo->count++;
	return p_in;
}

enum {H_STATS, H_RESET};

static String
EtherCount_read_param(Element *e, void *thunk)
{
  EtherCount *td = (EtherCount *)e;
  switch ((uintptr_t) thunk) {
  case H_STATS: {
    StringAccum sa;
    for (EtherCount::NIter iter = td->_neighbors.begin(); iter.live(); iter++) {
	    EtherCount::DstInfo n = iter.value();
	    sa << n._eth.unparse() << " " << n.count << "\n";
    }
    return sa.take_string();
  }

  default:
    return String();
  }

}

static int
EtherCount_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *)
{
  EtherCount *f = (EtherCount *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_RESET: f->_neighbors.clear(); return 0;
  }
  return 0;
}

void
EtherCount::add_handlers()
{
	add_read_handler("stats", EtherCount_read_param, H_STATS);
	add_write_handler("reset", EtherCount_write_param, H_RESET, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EtherCount)

