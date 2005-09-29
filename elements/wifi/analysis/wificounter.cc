/*
 * WifiCounter.{cc,hh} -- sets wifi txrate annotation on a packet
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

  DstInfo *nfo = _dsts.findp(src);
  if (!nfo) {
    _dsts.insert(src, DstInfo(src));
    nfo = _dsts.findp(src);
  }

  if (!nfo) {
    return p_in;
  }

  int type = wh->i_fc[0] & WIFI_FC0_TYPE_MASK;
  int subtype = (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) >> 4;
  
  int type_ndx = 3;
  switch (type) {
  case WIFI_FC0_TYPE_MGT: type_ndx = 0; break;
  case WIFI_FC0_TYPE_CTL: type_ndx = 1; break;
  case WIFI_FC0_TYPE_DATA: type_ndx = 2; break;
  }
  
  nfo->types[type_ndx][subtype].count++;
  nfo->types[type_ndx][subtype].bytes += p_in->length();
  
  nfo->totals.count++;
  nfo->totals.bytes += p_in->length();

  types[type_ndx][subtype].count++;
  types[type_ndx][subtype].bytes += p_in->length();
  
  totals.count++;
  totals.bytes += p_in->length();
  return p_in;
}

String
WifiCounter::stats() {
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

      sa << y << " " << types[x][y].s() << "\n";
    }
  }
  for (ETIter iter = _dsts.begin(); iter; iter++) {
    DstInfo nfo = iter.value();
    sa << nfo.eth.s().c_str() << " ";
    sa << nfo.totals.s();
    sa << "\n";
  }

  return sa.take_string();
}

enum {H_STATS};

static String
WifiCounter_read_param(Element *e, void *thunk)
{
  WifiCounter *td = (WifiCounter *)e;
  switch ((uintptr_t) thunk) {
  case H_STATS: return td->stats();
  default:
    return String();
  }
  
}  
	  
void
WifiCounter::add_handlers()
{
	add_read_handler("stats", WifiCounter_read_param, (void *) H_STATS);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(WifiCounter)

#include <click/hashmap.cc>
#include <click/vector.cc>
CLICK_ENDDECLS
