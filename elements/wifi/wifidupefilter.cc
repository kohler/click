/*
 * wifidupefilter.{cc,hh} -- Filters out duplicate packets based on
 * 802.11 sequence numbers.
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/args.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include "wifidupefilter.hh"

CLICK_DECLS

WifiDupeFilter::WifiDupeFilter()
  : _debug(false),
    _dupes(0)
{
}

WifiDupeFilter::~WifiDupeFilter()
{
}

int
WifiDupeFilter::configure(Vector<String> &conf, ErrorHandler* errh)
{
    return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
WifiDupeFilter::simple_action(Packet *p_in)
{
  click_wifi *w = (click_wifi *) p_in->data();

  if (p_in->length() < sizeof(click_wifi)) {
    return p_in;
  }

  EtherAddress src = EtherAddress(w->i_addr2);
  EtherAddress dst = EtherAddress(w->i_addr1);
  uint16_t seq = le16_to_cpu(w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
  uint8_t frag = le16_to_cpu(w->i_seq) & WIFI_SEQ_FRAG_MASK;
  u_int8_t more_frag = w->i_fc[1] & WIFI_FC1_MORE_FRAG;

  bool is_frag = frag || more_frag;

  DstInfo *nfo = _table.findp(src);

  if (w->i_fc[0] & WIFI_FC0_TYPE_CTL || dst.is_group()) {
    return p_in;
  }
  if (!nfo) {
    _table.insert(src, DstInfo(src));
    nfo = _table.findp(src);
    nfo->clear();
  }

  if (w->i_fc[1] & WIFI_FC1_RETRY && seq == nfo->seq &&
      (!is_frag || frag <= nfo->frag)) {
	  /* duplicate detected */
	  if (_debug) {
		  click_chatter("%p{element}: dup seq %d frag %d src %s\n",
				this,
				seq,
				frag,
				src.unparse().c_str());
	  }
	  nfo->_dupes++;
	  _dupes++;
	  p_in->kill();
	  return 0;
  }

  nfo->seq = seq;
  nfo->frag = frag;
  return p_in;
}


String
WifiDupeFilter::static_read_stats(Element *xf, void *)
{
  WifiDupeFilter *e = (WifiDupeFilter *) xf;
  StringAccum sa;

  for(DstTable::const_iterator i = e->_table.begin(); i.live(); i++) {
    DstInfo nfo = i.value();
    sa << nfo._eth;
    sa << " packets " << nfo._packets;
    sa << " dupes " << nfo._dupes;
    sa << " seq " << nfo.seq;
    sa << " frag " << nfo.frag;
    sa << "\n";

  }
  return sa.take_string();
}

enum {H_DEBUG, H_DUPES, H_RESET};

static String
WifiDupeFilter_read_param(Element *e, void *thunk)
{
  WifiDupeFilter *td = (WifiDupeFilter *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
      case H_DUPES:
	return String(td->_dupes) + "\n";
    default:
      return String();
    }
}
static int
WifiDupeFilter_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WifiDupeFilter *f = (WifiDupeFilter *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!BoolArg().parse(s, debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_RESET: {
    f->_table.clear();
    f->_dupes = 0;
  }
  }
  return 0;
}
void
WifiDupeFilter::add_handlers()
{
  add_read_handler("debug", WifiDupeFilter_read_param, H_DEBUG);
  add_read_handler("dupes", WifiDupeFilter_read_param, H_DUPES);
  add_read_handler("drops", WifiDupeFilter_read_param, H_DUPES);

  add_write_handler("debug", WifiDupeFilter_write_param, H_DEBUG);
  add_write_handler("reset", WifiDupeFilter_write_param, H_RESET, Handler::BUTTON);

  add_read_handler("stats", static_read_stats, 0);
}

EXPORT_ELEMENT(WifiDupeFilter)
CLICK_ENDDECLS

