/*
 * autoratefallback.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include <elements/wifi/availablerates.hh>
#include "autoratefallback.hh"

CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

AutoRateFallback::AutoRateFallback()
  : Element(2, 1),
    _stepup(0),
    _stepdown(0),
    _packet_size_threshold(0)
{
  MOD_INC_USE_COUNT;

  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

AutoRateFallback::~AutoRateFallback()
{
  MOD_DEC_USE_COUNT;
}

int
AutoRateFallback::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int ret = cp_va_parse(conf, this, errh,
			cpKeywords, 
			"STEPUP", cpInteger, "0-100", &_stepup,
			"STEPDOWN", cpInteger, "0-100", &_stepdown,
			"RT", cpElement, "availablerates", &_rtable,
			"THRESHOLD", cpUnsigned, "xxx", &_packet_size_threshold,
			0);
  return ret;
}

void
AutoRateFallback::process_feedback(Packet *p_in)
{
  assert(p_in);
  if (!p_in) {
    return;
  }
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  int status = WIFI_TX_STATUS_ANNO(p_in);  
  int rate = WIFI_RATE_ANNO(p_in);

  struct timeval now;
  click_gettimeofday(&now);

  if (dst == _bcast) {
    /* don't record info for bcast packets */
    return;
  }

  if (0 == rate) {
    /* rate wasn't set */
    return;
  }
  if (!status && p_in->length() < _packet_size_threshold) {
    /* 
     * don't deal with short packets, 
     * since they can skew what rate we should be at 
     */
    return;
  }

  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    return;
  }


  if (nfo->pick_rate() != rate) {
    return;
  }


  if (status == 0) {
    nfo->_successes++;
    if (nfo->_successes > _stepup && 
	nfo->_current_index != nfo->_rates.size() - 1) {
      if (nfo->_rates.size()) {
	click_chatter("%{element} steping up for %s from %d to %d\n",
		      this,
		      nfo->_eth.s().cc(),
		      nfo->_rates[nfo->_current_index],
		      nfo->_rates[min(nfo->_rates.size() - 1, 
				      nfo->_current_index + 1)]);
      }
      nfo->_current_index = min(nfo->_current_index + 1, nfo->_rates.size() - 1);
      
    }
  } else {
    if (nfo->_rates.size()) {
      click_chatter("%{element} steping down for %s from %d to %d\n",
		    this,
		    nfo->_eth.s().cc(),
		    nfo->_rates[nfo->_current_index],
		    nfo->_rates[max(0, nfo->_current_index - 1)]);
    }
    nfo->_successes = 0;
    nfo->_current_index = max(nfo->_current_index - 1, 0);
  }
  return;
}


void
AutoRateFallback::assign_rate(Packet *p_in)
  {
    assert(p_in);
  if (!p_in) {
    click_chatter("%{element} ah, !p_in\n",
		  this);
    return;
  }

  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  SET_WIFI_FROM_CLICK(p_in);

  if (dst == _bcast) {
    SET_WIFI_RATE_ANNO(p_in, 2);
    return;
  }
  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    _neighbors.insert(dst, DstInfo(dst));
    nfo = _neighbors.findp(dst);
    nfo->_rates = _rtable->lookup(dst);
    nfo->_successes = 0;
    /* initial to max? */
    nfo->_current_index = nfo->_rates.size() - 1;
    click_chatter("%{element} initial rate for %s is %d\n",
		  this,
		  nfo->_eth.s().cc(),
		  nfo->_rates[nfo->_current_index]);
  }

  int rate = nfo->pick_rate();
  SET_WIFI_RATE_ANNO(p_in, rate);
  SET_WIFI_MAX_RETRIES_ANNO(p_in, 4);
  SET_WIFI_ALT_RATE_ANNO(p_in, 2);
  SET_WIFI_ALT_MAX_RETRIES_ANNO(p_in, 4);
  return;
  
}


Packet *
AutoRateFallback::pull(int port)
{
  Packet *p = input(port).pull();
  if (p) {
    assign_rate(p);
  }
  return p;
}

void
AutoRateFallback::push(int port, Packet *p_in)
{
  if (!p_in) {
    return;
  }
  if (port != 0) {
    process_feedback(p_in);
    p_in->kill();
  } else {
    assign_rate(p_in);
    output(port).push(p_in);
  }
}


String
AutoRateFallback::print_rates() 
{
  return "";
}


enum {H_DEBUG, H_STEPUP, H_STEPDOWN, H_THRESHOLD, H_RATES, H_RESET};


static String
AutoRateFallback_read_param(Element *e, void *thunk)
{
  AutoRateFallback *td = (AutoRateFallback *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_STEPDOWN:
    return String(td->_stepdown) + "\n";
  case H_STEPUP:
    return String(td->_stepup) + "\n";
  case H_THRESHOLD:
    return String(td->_packet_size_threshold) + "\n";
  case H_RATES: {
    return td->print_rates();
  }
  default:
    return String();
  }
}
static int 
AutoRateFallback_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AutoRateFallback *f = (AutoRateFallback *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_STEPUP: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("stepup parameter must be unsigned");
    f->_stepup = m;
    break;
  }
  case H_STEPDOWN: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("stepdown parameter must be unsigned");
    f->_stepdown = m;
    break;
  }
  case H_THRESHOLD: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("threshold parameter must be unsigned");
    f->_packet_size_threshold = m;
    break;
  }
  case H_RESET: 
    f->_neighbors.clear();
    break;
  }
  return 0;
}


void
AutoRateFallback::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", AutoRateFallback_read_param, (void *) H_DEBUG);
  add_read_handler("rates", AutoRateFallback_read_param, (void *) H_RATES);
  add_read_handler("threshold", AutoRateFallback_read_param, (void *) H_THRESHOLD);
  add_read_handler("stepup", AutoRateFallback_read_param, (void *) H_STEPUP);
  add_read_handler("stepdown", AutoRateFallback_read_param, (void *) H_STEPDOWN);

  add_write_handler("debug", AutoRateFallback_write_param, (void *) H_DEBUG);
  add_write_handler("threshold", AutoRateFallback_write_param, (void *) H_THRESHOLD);
  add_write_handler("stepup", AutoRateFallback_write_param, (void *) H_STEPUP);
  add_write_handler("stepdown", AutoRateFallback_write_param, (void *) H_STEPDOWN);
  add_write_handler("reset", AutoRateFallback_write_param, (void *) H_RESET);

}
// generate Vector template instance
#include <click/bighashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, AutoRateFallback::DstInfo>;
template class DEQueue<AutoRateFallback::tx_result>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(AutoRateFallback)

