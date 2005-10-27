/*
 * WifiCounter.{cc,hh} -- count source and destinations of 802.11 packets
 * John Bicket
 *
 * Copyright (c) 2005 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include "wificounter.hh"
CLICK_DECLS

WifiCounter::WifiCounter()
{
}

WifiCounter::~WifiCounter()
{
}

int
WifiCounter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  cpEnd) < 0) {
    return -1;
  }

  return 0;
}

Packet *
WifiCounter::simple_action (Packet *p_in)
{

  click_wifi *wh = (click_wifi *) p_in->data();

  EtherAddress src = EtherAddress(wh->i_addr2);
  EtherAddress dst = EtherAddress(wh->i_addr1);

  
  int type = wh->i_fc[0] & WIFI_FC0_TYPE_MASK;
  int subtype = (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) >> 4;
  totals.count++;
  totals.bytes += p_in->length();
  
  int type_ndx = 3;
  switch (type) {
  case WIFI_FC0_TYPE_MGT: type_ndx = 0; break;
  case WIFI_FC0_TYPE_CTL: type_ndx = 1; break;
  case WIFI_FC0_TYPE_DATA: type_ndx = 2; break;
	  break;
  }

  types[type_ndx][subtype].count++;
  types[type_ndx][subtype].bytes += p_in->length();

  totals.count++;
  totals.bytes += p_in->length();

  EtherPair pair = EtherPair(src, dst);
  EtherPairCount *c = _pairs.findp(pair);
  if (!c) {
	  _pairs.insert(pair, EtherPairCount(pair));
	  c = _pairs.findp(pair);
  }

  c->_count++;

  return p_in;
}

enum {H_STATS, H_TYPES};

static String
WifiCounter_read_param(Element *e, void *thunk)
{
  WifiCounter *td = (WifiCounter *)e;
  switch ((uintptr_t) thunk) {
  case H_STATS: {
	StringAccum sa;
	for (WifiCounter::ETIter iter = td->_pairs.begin(); iter; iter++) {
		WifiCounter::EtherPairCount c = iter.value();
		sa << c._pair._src.s().c_str() << " ";
		sa << c._pair._dst.s().c_str() << " ";
		sa << c._count;
		sa << "\n";
	}
	return sa.take_string();

  }
  case H_TYPES: {
	StringAccum sa;

	for (int x = 0; x < 3; x++) {
		for (int y = 0; y < 16; y++) {
			sa << "totals: ";
			if (x == 0) {
				sa << " mgt ";
			} else if (x == 1) {
				sa << " ctl ";
			} else if (x == 2) {
				sa << " data ";
			}
			
			sa << y << " " << td->types[x][y].s() << "\n";
		}
	}
	return sa.take_string();
  }
  default:
    return String();
  }  
}  
	  
void
WifiCounter::add_handlers()
{
	add_read_handler("stats", WifiCounter_read_param, (void *) H_STATS);
	add_read_handler("types", WifiCounter_read_param, (void *) H_TYPES);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(WifiCounter)

#include <click/hashmap.cc>
#include <click/vector.cc>
CLICK_ENDDECLS
