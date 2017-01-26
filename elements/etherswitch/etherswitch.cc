/*
 * etherswitch.{cc,hh} -- learning, forwarding Ethernet bridge
 * John Jannotti
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
#include "etherswitch.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
CLICK_DECLS

EtherSwitch::EtherSwitch()
    : _table(AddrInfo(-1, Timestamp())), _timeout(300)
{
}

EtherSwitch::~EtherSwitch()
{
    _table.clear();
}

int
EtherSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read("TIMEOUT", SecondsArg(), _timeout)
	.complete();
}

void
EtherSwitch::broadcast(int source, Packet *p)
{
  int n = noutputs();
  assert((unsigned) source < (unsigned) n);
  int sent = 0;
  for (int i = n - 1; i >=0 ; i--)
    if (i != source) {
      Packet *pp = (sent < n - 2 ? p->clone() : p);
      output(i).push(pp);
      sent++;
    }
  assert(sent == n - 1);
}

void
EtherSwitch::push(int source, Packet *p)
{
    click_ether* e = (click_ether*) p->data();
    int outport = -1;		// Broadcast

    // 0 timeout means dumb switch
    if (_timeout != 0) {
	_table.set(EtherAddress(e->ether_shost), AddrInfo(source, p->timestamp_anno()));

	// Set outport if dst is unicast, we have info about it, and the
	// info is still valid.
	EtherAddress dst(e->ether_dhost);
	if (!dst.is_group()) {
	    if (Table::iterator dst_info = _table.find(dst)) {
		if (p->timestamp_anno() < dst_info.value().stamp + Timestamp(_timeout, 0))
		    outport = dst_info.value().port;
		else
		    _table.erase(dst_info);
	    }
	}
    }

  if (outport < 0)
    broadcast(source, p);
  else if (outport == source)	// Don't send back out on same interface
    p->kill();
  else				// forward
    output(outport).push(p);
}

String
EtherSwitch::reader(Element* f, void *thunk)
{
    EtherSwitch* sw = (EtherSwitch*)f;
    switch ((intptr_t) thunk) {
    case 0: {
	StringAccum sa;
	for (Table::iterator iter = sw->_table.begin(); iter.live(); iter++)
	    sa << iter.key() << ' ' << iter.value().port << '\n';
	return sa.take_string();
    }
    case 1:
	return String(sw->_timeout);
    default:
	return String();
    }
}

int
EtherSwitch::writer(const String &s, Element *e, void *, ErrorHandler *errh)
{
    EtherSwitch *sw = (EtherSwitch *) e;
    if (!SecondsArg().parse_saturating(s, sw->_timeout))
	return errh->error("expected timeout (integer)");
    return 0;
}

void
EtherSwitch::add_handlers()
{
    add_read_handler("table", reader, 0);
    add_read_handler("timeout", reader, 1);
    add_write_handler("timeout", writer, 0);
}

EXPORT_ELEMENT(EtherSwitch)
CLICK_ENDDECLS
