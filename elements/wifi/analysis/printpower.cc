/*
 * printpower.{cc,hh} -- print power levels of a packet
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
#include "printpower.hh"
CLICK_DECLS

PrintPower::PrintPower()
{
}

PrintPower::~PrintPower()
{
}

int
PrintPower::configure(Vector<String> &conf, ErrorHandler*errh)
{
	_print = false;
	_power = 0;
	int ret = cp_va_parse(conf, this, errh,
			      cpKeywords,
			      "PRINT", cpBool, "xxx", &_print,
			      0);
	return ret;
}

Packet *
PrintPower::simple_action(Packet *p)
{
	struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();
	if (_print) {
		click_chatter("%d\n", ceh->power);
	}
	_power = ceh->power;
    
	return p;
}

enum {H_POWER};

static String
read_param(Element *e, void *thunk)
{
	PrintPower *f = (PrintPower *)e;
	switch((int)thunk) {
	case H_POWER: return String(f->_power) + "\n";
	}
	return String();
}

void
PrintPower::add_handlers()
{
	add_read_handler("power", read_param, (void *)H_POWER);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(PrintPower)
