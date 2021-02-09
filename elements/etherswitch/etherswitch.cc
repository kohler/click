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
#include <click/confparse.hh>
#include <click/string.hh>
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
    if (Args(conf, this, errh)
	.read("TIMEOUT", SecondsArg(), _timeout)
	.complete() < 0)
        return -1;

    int n = noutputs();
    _pfrs.resize(n);
    for (int i = 0; i < n; i++)
        _pfrs[i].configure(i, n);
    return 0;
}

void
EtherSwitch::broadcast(int source, Packet *p)
{
  PortForwardRule &pfr = _pfrs[source];
  int n = pfr.bv.size();
  int w = pfr.w;
  assert((unsigned) w <= (unsigned) n);
  for (int i = 0; i < n && w > 0; i++) {
    if (pfr.bv[i]) {
      Packet *pp = (w > 1 ? p->clone() : p);
      output(i).push(pp);
      w--;
    }
  }
}

int
EtherSwitch::remove_port_forwarding(String portmaps, ErrorHandler *errh)
{
    int nn = noutputs();
    Vector<PortForwardRule> new_pfrs(_pfrs);
    Vector<String> maps_vec;
    cp_argvec(portmaps, maps_vec);
    Vector<String>::const_iterator i,n;
    for (i = maps_vec.begin(), n = maps_vec.end(); i != n; ++i) {
        int source, outport;
        Args args = Args(this,errh).push_back_words(*i);
        if (args.read_mp("SOURCE", source).consume() < 0)
            return -1;
        if (source < 0 || source > nn)
            return -1;
        int new_pfr_n = (new_pfrs[source].bv).size();
        while (!args.empty()) {
            if (args.read_p("OUTPORT", outport).consume() < 0)
                return -1;
            if (outport < 0 || outport > new_pfr_n)
                return -1;
            (new_pfrs[source].bv)[outport] = false;
        }
        new_pfrs[source].calculate_weight();
    }
    _pfrs = new_pfrs;
    return 0;
}

void
EtherSwitch::reset_port_forwarding()
{
    int n = noutputs();
    for (int i = 0; i < n; i++)
        _pfrs[i].configure(i, n);
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
  else if ((_pfrs[source].bv)[outport])	// forward w/ filter
      output(outport).push(p);
  else
      p->kill();
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
    case 2: {
	StringAccum sa;
	int n = sw->noutputs();
	for (int i = 0; i < n; i++)
		sa << "weight: " << (sw->_pfrs[i].w) << "\t"
		   << i << ": " << (sw->_pfrs[i].bv).unparse() << '\n';
	return sa.take_string();
    }
    default:
	return String();
    }
}

int
EtherSwitch::writer(const String &s, Element *e, void *thunk, ErrorHandler *errh)
{
    EtherSwitch *sw = (EtherSwitch *) e;
    switch((intptr_t) thunk) {
    case 0: {
        if (!SecondsArg().parse_saturating(s, sw->_timeout)) {
            return errh->error("expected timeout (integer)");
        }
        break;
    }
    case 1: {
        if (sw->remove_port_forwarding(s, errh) < 0) {
            return errh->error("invalid port forwarding");
        }
        break;
    }
    case 2: {
        sw->reset_port_forwarding();
        break;
    }
    default:
        return errh->error("bad thunk");
    }
    return 0;
}

void
EtherSwitch::add_handlers()
{
    add_read_handler("table", reader, 0);
    add_read_handler("timeout", reader, 1);
    add_read_handler("port_forwarding", reader, 2);
    add_write_handler("timeout", writer, 0);
    add_write_handler("remove_port_forwarding", writer, 1);
    add_write_handler("reset_port_forwarding", writer, 2);
}

EXPORT_ELEMENT(EtherSwitch)
CLICK_ENDDECLS
