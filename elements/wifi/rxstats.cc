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

int
RXStats::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  cpEnd) < 0) {
    return -1;
  }

  return 0;
}

Packet *
RXStats::simple_action(Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress src = EtherAddress(eh->ether_shost);

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


  _sum_signal += signal;
  _sum_noise += noise;
  _packets++;

  return p_in;
}

enum {H_STATS, H_SIGNAL, H_NOISE};

static String
RXStats_read_param(Element *e, void *thunk)
{
  RXStats *td = (RXStats *)e;
  switch ((uintptr_t) thunk) {
  case H_STATS: {
    struct timeval now;
    click_gettimeofday(&now);
    
    StringAccum sa;
    for (RXStats::NIter iter = td->_neighbors.begin(); iter; iter++) {
      RXStats::DstInfo n = iter.value();
      struct timeval age = now - n._last_received;
      sa << n._eth.s().cc();
      sa << " rate: " << n._rate;
      sa << " signal: " << n._signal;
      sa << " noise: " << n._noise;
      sa << " last_received: " << age << "\n";
    }
    return sa.take_string();
  }
  case H_SIGNAL: 
    return td->_packets ? String(td->_sum_signal/td->_packets) : String((int)0);
  case H_NOISE:
    return td->_packets ? String(td->_sum_noise/td->_packets) : String((int)0);
  default:
    return String();
  }
  
}  
	  
void
RXStats::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("stats", RXStats_read_param, (void *) H_STATS);
  add_read_handler("signal", RXStats_read_param, (void *) H_SIGNAL);
  add_read_handler("noise", RXStats_read_param, (void *) H_NOISE);

}
// generate Vector template instance
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, RXStats::DstInfo>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(RXStats)

