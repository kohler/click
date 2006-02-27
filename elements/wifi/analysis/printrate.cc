/*
 * printrate.{cc,hh} -- print rate levels of a packet
 * John Bicket
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include "printrate.hh"
CLICK_DECLS

PrintRate::PrintRate()
{
}

PrintRate::~PrintRate()
{
}

int
PrintRate::configure(Vector<String> &conf, ErrorHandler*errh)
{
	_print = false;
	_rate = 0;
	int ret = cp_va_parse(conf, this, errh,
			      cpKeywords,
			      "PRINT", cpBool, "xxx", &_print,
			      0);
	return ret;
}

Packet *
PrintRate::simple_action(Packet *p)
{
	struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();
	if (_print) {
		click_chatter("%d\n", ceh->rate);
	}
	_rate = ceh->rate;
    
	return p;
}

enum {H_RATE};

static String
read_param(Element *e, void *thunk)
{
	PrintRate *f = (PrintRate *)e;
	switch((intptr_t)thunk) {
	case H_RATE: {
		StringAccum sa;
		sa << f->_rate << "\n";
		return sa.take_string();
	}
	}
	return String();
}

void
PrintRate::add_handlers()
{
	add_read_handler("rate", read_param, (void *)H_RATE);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(PrintRate)
