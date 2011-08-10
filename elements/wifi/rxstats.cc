/*
 * rxstats.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "rxstats.hh"
CLICK_DECLS

RXStats::RXStats()
{
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

RXStats::~RXStats()
{
}

Packet *
RXStats::simple_action(Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress src = EtherAddress(eh->ether_shost);
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);

  DstInfo *nfo = _neighbors.findp(src);
  if (!nfo) {
    DstInfo foo = DstInfo(src);
    _neighbors.insert(src, foo);
    nfo = _neighbors.findp(src);
  }

  nfo->_rate = ceh->rate;
  nfo->_signal = ceh->rssi;
  nfo->_noise = ceh->silence;

  nfo->_packets++;
  nfo->_sum_signal += ceh->rssi;
  nfo->_sum_noise += ceh->silence;
  nfo->_last_received.assign_now();

  return p_in;
}

enum {H_STATS, H_RESET};

static String
RXStats_read_param(Element *e, void *thunk)
{
  RXStats *td = (RXStats *)e;
  switch ((uintptr_t) thunk) {
  case H_STATS: {
    Timestamp now = Timestamp::now();

    StringAccum sa;
    for (RXStats::NIter iter = td->_neighbors.begin(); iter.live(); iter++) {
      RXStats::DstInfo n = iter.value();
      Timestamp age = now - n._last_received;
      Timestamp avg_signal;
      Timestamp avg_noise;
      if (n._packets) {
	      avg_signal = Timestamp::make_msec(1000*n._sum_signal / n._packets);
	      avg_noise = Timestamp::make_msec(1000*n._sum_noise / n._packets);
      }
      sa << n._eth.unparse();
      sa << " rate " << n._rate;
      sa << " signal " << n._signal;
      sa << " noise " << n._noise;
      sa << " avg_signal " << avg_signal;
      sa << " avg_noise " << avg_noise;
      sa << " total_signal " << n._sum_signal;
      sa << " total_noise " << n._sum_noise;
      sa << " packets " << n._packets;
      sa << " last_received " << age << "\n";
    }
    return sa.take_string();
  }

  default:
    return String();
  }

}

static int
RXStats_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *)
{
  RXStats *f = (RXStats *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_RESET: f->_neighbors.clear(); return 0;
  }
  return 0;
}

void
RXStats::add_handlers()
{
  add_read_handler("stats", RXStats_read_param, H_STATS);
  add_write_handler("reset", RXStats_write_param, H_RESET, Handler::BUTTON);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(RXStats)

