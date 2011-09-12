/*
 * availablerates.{cc,hh} -- Poor man's arp table
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
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "availablerates.hh"
CLICK_DECLS

AvailableRates::AvailableRates()
{

  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);

}

AvailableRates::~AvailableRates()
{
}

void *
AvailableRates::cast(const char *n)
{
  if (strcmp(n, "AvailableRates") == 0)
    return (AvailableRates *) this;
  else
    return 0;
}

int
AvailableRates::parse_and_insert(String s, ErrorHandler *errh)
{
  EtherAddress e;
  Vector<int> rates;
  Vector<String> args;
  cp_spacevec(s, args);
  if (args.size() < 2) {
    return errh->error("error param %s must have > 1 arg", s.c_str());
  }
  bool default_rates = false;
  if (args[0] == "DEFAULT") {
    default_rates = true;
    _default_rates.clear();
  } else {
      if (!EtherAddressArg().parse(args[0], e))
      return errh->error("error param %s: must start with ethernet address", s.c_str());
  }

  for (int x = 1; x< args.size(); x++) {
    int r;
    IntArg().parse(args[x], r);
    if (default_rates) {
      _default_rates.push_back(r);
    } else {
      rates.push_back(r);
    }
  }

  if (default_rates) {
    return 0;
  }

  DstInfo d = DstInfo(e);
  d._rates = rates;
  d._eth = e;
  _rtable.insert(e, d);
  return 0;
}
int
AvailableRates::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = 0;
  _debug = false;
  for (int x = 0; x < conf.size(); x++) {
    res = parse_and_insert(conf[x], errh);
    if (res != 0) {
      return res;
    }
  }

  return res;
}

void
AvailableRates::take_state(Element *e, ErrorHandler *)
{
  AvailableRates *q = (AvailableRates *)e->cast("AvailableRates");
  if (!q) return;
  _rtable = q->_rtable;
  _default_rates = _default_rates;

}

Vector<int>
AvailableRates::lookup(EtherAddress eth)
{
  if (!eth) {
    click_chatter("%s: lookup called with NULL eth!\n", name().c_str());
    return Vector<int>();
  }

  DstInfo *dst = _rtable.findp(eth);
  if (dst) {
    return dst->_rates;
  }

  if (_default_rates.size()) {
    return _default_rates;
  }

  return Vector<int>();
}

int
AvailableRates::insert(EtherAddress eth, Vector<int> rates)
{
  if (!(eth)) {
    if (_debug) {
      click_chatter("AvailableRates %s: You fool, you tried to insert %s\n",
		    name().c_str(),
		    eth.unparse().c_str());
    }
    return -1;
  }
  DstInfo *dst = _rtable.findp(eth);
  if (!dst) {
    _rtable.insert(eth, DstInfo(eth));
    dst = _rtable.findp(eth);
  }
  dst->_eth = eth;
  dst->_rates.clear();
  if (_default_rates.size()) {
    /* only add rates that are in the default rates */
    for (int x = 0; x < rates.size(); x++) {
      for (int y = 0; y < _default_rates.size(); y++) {
	if (rates[x] == _default_rates[y]) {
	  dst->_rates.push_back(rates[x]);
	}
      }
    }
  } else {
    dst->_rates = rates;
  }
  return 0;
}



enum {H_DEBUG, H_INSERT, H_REMOVE, H_RATES};


static String
AvailableRates_read_param(Element *e, void *thunk)
{
  AvailableRates *td = (AvailableRates *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_RATES: {
    StringAccum sa;
    if (td->_default_rates.size()) {
      sa << "DEFAULT ";
      for (int x = 0; x < td->_default_rates.size(); x++) {
	sa << " " << td->_default_rates[x];
      }
      sa << "\n";
    }
    for (AvailableRates::RIter iter = td->_rtable.begin(); iter.live(); iter++) {
      AvailableRates::DstInfo n = iter.value();
      sa << n._eth.unparse() << " ";
      for (int x = 0; x < n._rates.size(); x++) {
	sa << " " << n._rates[x];
      }
      sa << "\n";
    }
    return sa.take_string();
  }
  default:
    return String();
  }
}
static int
AvailableRates_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AvailableRates *f = (AvailableRates *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!BoolArg().parse(s, debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_INSERT:
    return f->parse_and_insert(in_s, errh);
  case H_REMOVE: {
    EtherAddress e;
    if (!EtherAddressArg().parse(s, e))
      return errh->error("remove parameter must be ethernet address");
    f->_rtable.erase(e);
    break;
  }

  }
  return 0;
}

void
AvailableRates::add_handlers()
{
  add_read_handler("debug", AvailableRates_read_param, H_DEBUG);
  add_read_handler("rates", AvailableRates_read_param, H_RATES);


  add_write_handler("debug", AvailableRates_write_param, H_DEBUG);
  add_write_handler("insert", AvailableRates_write_param, H_INSERT);
  add_write_handler("remove", AvailableRates_write_param, H_REMOVE);


}

CLICK_ENDDECLS
EXPORT_ELEMENT(AvailableRates)

