/*
 * autotxrate.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "autotxrate.hh"
CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

static inline int next_lower_rate(int rate) {
    switch(rate) {
    case 1:
      return 1;
    case 2:
      return 1;
    case 5:
      return 2;
    case 11:
      return 5;
    default:
      return rate;
    }
  }


static inline int next_higher_rate(int rate) {
    switch(rate) {
    case 1:
      return 2;
    case 2:
      return 5;
    case 5:
      return 11;
    case 11:
      return 11;
    default:
      return rate;
    }
  }
  

AutoTXRate::AutoTXRate()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;

  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

AutoTXRate::~AutoTXRate()
{
  MOD_DEC_USE_COUNT;
}

AutoTXRate *
AutoTXRate::clone() const
{
  return new AutoTXRate;
}

int
AutoTXRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "Probation count (packets)", &_max_probation_count,
		  cpKeywords, 
		  0) < 0) {
    return -1;
  }

  return 0;
}

/* returns 0 if we haven't gotten feedback for a dst */
int 
AutoTXRate::get_tx_rate(EtherAddress dst)
{
  if (dst == _bcast) {
    return 1;
  }
  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    return 0;
  }
  return nfo->_rate;

}
Packet *
AutoTXRate::simple_action(Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  int success = WIFI_TX_SUCCESS_ANNO(p_in);
  int rate = WIFI_RATE_ANNO(p_in);
  int retries = WIFI_RETRIES_ANNO(p_in);
  if (dst == _bcast) {
    /* don't record info for bcast packets */
    p_in->kill();
    return 0;
  }

  if (0 == rate) {
    /* rate wasn't set */
    p_in->kill();
    return 0;
  }


  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    DstInfo foo = DstInfo(dst);
    _neighbors.insert(dst, foo);
    nfo = _neighbors.findp(dst);
  }

  if (success) {

    if (!nfo->_rate || rate >= nfo->_rate) {
      //click_chatter("AutoTXRate: packet to %s succeeded, %d retries, %d rate", 
      //dst.s().cc(), retries, rate);
      click_gettimeofday(&nfo->_last_success);
      nfo->_packets++;
      if (!nfo->_rate) {
	nfo->_rate = min(rate, nfo->_rate);
      }
      if (nfo->_packets > _max_probation_count) {
	int next_rate = next_higher_rate(nfo->_rate);
	if (next_rate == nfo->_rate) {
	  nfo->_packets++;
	} else {
	  nfo->_rate = next_rate;
	  nfo->_packets = 0;
	}

      }
    }
    p_in->kill();
    return 0;
  }

  /* the packet has failed */
  if (WIFI_RETRIES_ANNO(p_in) > 4) {
    //click_chatter("packet to %s failed!", dst.s().cc());
    p_in->kill();
    return 0;
  }

  if (rate > 1) {
    /* try twice at a speed before bumping down */
    if (rate == nfo->_rate) {
      nfo->_rate = next_lower_rate(nfo->_rate);
    }
    //click_chatter("AutoTXRate: packet to %s failed twice, bumping down to %d\n", 
    //dst.s().cc(), nfo->_rate);
    
    SET_WIFI_RATE_ANNO(p_in, nfo->_rate);
    SET_WIFI_RETRIES_ANNO(p_in, 0);
    nfo->_packets = 0;
  }
  SET_WIFI_RETRIES_ANNO(p_in, retries+1);


  return p_in;
}
String
AutoTXRate::static_print_stats(Element *e, void *)
{
  AutoTXRate *n = (AutoTXRate *) e;
  return n->print_stats();
}

String
AutoTXRate::print_stats() 
{
  struct timeval now;
  click_gettimeofday(&now);
  
  StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter; iter++) {
    DstInfo n = iter.value();
    struct timeval age = now - n._last_success;
    sa << n._eth.s().cc() << " ";
    sa << "rate: " << n._rate << " ";
    sa << "packets: " << n._packets << " ";
    sa << "last_success: " << age << "\n";
  }
  return sa.take_string();
}
void
AutoTXRate::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("stats", static_print_stats, 0);

}
// generate Vector template instance
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<EtherAddress, AutoTXRate::DstInfo>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(AutoTXRate)

