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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "rxstats.hh"
CLICK_DECLS

RXStats::RXStats()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

RXStats::~RXStats()
{
  MOD_DEC_USE_COUNT;
}

RXStats *
RXStats::clone() const
{
  return new RXStats;
}

int
RXStats::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  0) < 0) {
    return -1;
  }

  return 0;
}

Packet *
RXStats::simple_action(Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress src = EtherAddress(eh->ether_shost);
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  /* ignore broadcast packets, they're all at 1Mb */
  if (dst != _bcast) {
    int rate = WIFI_RATE_ANNO(p_in);
    int signal = WIFI_SIGNAL_ANNO(p_in);
    int noise = WIFI_NOISE_ANNO(p_in);
    
    DstInfo *nfo = _neighbors.findp(src);
    if (!nfo) {
      DstInfo foo = DstInfo(src);
      _neighbors.insert(src, foo);
      nfo = _neighbors.findp(src);
    }
    
    nfo->_rate = rate;
    nfo->_signal = signal;
    nfo->_noise = noise;
    click_gettimeofday(&nfo->_last_received);
  }

  return p_in;
}
String
RXStats::static_print_stats(Element *e, void *)
{
  RXStats *n = (RXStats *) e;
  return n->print_stats();
}

String
RXStats::print_stats() 
{
  struct timeval now;
  click_gettimeofday(&now);
  
  StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter; iter++) {
    DstInfo n = iter.value();
    struct timeval age = now - n._last_received;
    sa << n._eth.s().cc() << " ";
    sa << "rate: " << n._rate << " ";
    sa << "signal: " << n._signal << " ";
    sa << "noise: " << n._noise << " ";
    sa << "last_received: " << age << "\n";
  }
  return sa.take_string();
}
void
RXStats::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("stats", static_print_stats, 0);

}
// generate Vector template instance
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<EtherAddress, RXStats::DstInfo>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(RXStats)

