/*
 * probetxrate.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "probetxrate.hh"
#include <elements/wifi/availablerates.hh>
CLICK_DECLS

#define PROBE_MAX_RETRIES 6

ProbeTXRate::ProbeTXRate()
  : Element(2, 1),
    _offset(0),
    _packet_size_threshold(0),
    _rate_window_ms(0),
    _rtable(0)
{
  MOD_INC_USE_COUNT;

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

ProbeTXRate::~ProbeTXRate()
{
  MOD_DEC_USE_COUNT;
}

int
ProbeTXRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _filter_low_rates = false;
  _filter_never_success = false;
  _aggressive_alt_rate = false;
  _debug = false;
  _alt_rate = true;
  _active = true;
  _original_retries = 4;
  _min_sample = 20;
  int ret = cp_va_parse(conf, this, errh,
			cpKeywords, 
			"OFFSET", cpUnsigned, "offset", &_offset,
			"WINDOW", cpUnsigned, "window", &_rate_window_ms,
			"DEBUG", cpBool, "debug", &_debug,
			"THRESHOLD", cpUnsigned, "window", &_packet_size_threshold,
			"RT", cpElement, "availablerates", &_rtable,
			"FILTER_LOW_RATES", cpBool, "foo", &_filter_low_rates,
			"FILTER_NEVER_SUCCESS", cpBool, "foo", &_filter_never_success,
			"AGGRESSIVE_ALT_RATE", cpBool, "foo", &_aggressive_alt_rate,
			"ALT_RATE", cpBool, "xxx", &_alt_rate,
			"ORIGINAL_RETRIES", cpUnsigned, "foo", &_original_retries,
			"MIN_SAMPLE", cpUnsigned, "foo", &_min_sample,
			"ACTIVE", cpBool, "xxx", &_active,
			cpEnd);
  if (ret < 0) {
    return ret;
  }
  if (_rate_window_ms <= 0) 
    return errh->error("WINDOW must be > 0");

  if (!_rtable || _rtable->cast("AvailableRates") == 0) 
    return errh->error("AvailableRates element is not provided or not a AvailableRates");


  _rate_window.tv_sec = _rate_window_ms / 1000;
  _rate_window.tv_usec = (_rate_window_ms % 1000) * 1000;

  return ret;
}




void
ProbeTXRate::assign_rate(Packet *p_in) {

  if (!p_in) {
    click_chatter("%{element} ah, !p_in\n",
		  this);
    return;
  }

  uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
  EtherAddress dst = EtherAddress(dst_ptr);
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p_in->all_user_anno();

  if (dst.is_group()) {
    Vector<int> rates = _rtable->lookup(_bcast);
    if (rates.size()) {
      ceh->rate = rates[0];
    } else {
      ceh->rate = 2;
    }
    return;
  }
  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo || !nfo->_rates.size()) {
    _neighbors.insert(dst, DstInfo(dst, _rtable->lookup(dst)));
    nfo = _neighbors.findp(dst);
  }

  struct timeval now;
  struct timeval old;
  click_gettimeofday(&now);
  timersub(&now, &_rate_window, &old);
  nfo->trim(old);
  //nfo->check();
  ceh->rate = nfo->pick_rate(_min_sample);
  ceh->max_retries = PROBE_MAX_RETRIES;
  ceh->alt_rate = 0;
  ceh->alt_max_retries = 0;

  return;
}


void
ProbeTXRate::process_feedback(Packet *p_in) {

  if (!p_in) {
    return;
  }
  uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
  EtherAddress dst = EtherAddress(dst_ptr);
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p_in->all_user_anno();
  bool success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);
  bool used_alt_rate = ceh->flags & WIFI_EXTRA_TX_USED_ALT_RATE;
  int retries = _alt_rate ? MIN(_original_retries, ceh->retries) :
    ceh->retries;
  int alt_retries = ceh->retries - retries;

  struct timeval now;
  click_gettimeofday(&now);

  if (dst == _bcast) {
    /* don't record info for bcast packets */
    if (_debug) {
      click_chatter("%{element}: discarding bcast %s\n",
		    this,
		    dst.s().cc());
    }
    return;
  }

  if (0 == ceh->rate) {
    /* rate wasn't set */
    if (_debug) {
          click_chatter("%{element} no rate set for %s\n",
			this,
			dst.s().cc());
    }
    return;
  }
  
  if (!success && p_in->length() < _packet_size_threshold) {
    /* 
     * don't deal with short packets, 
     * since they can skew what rate
     * we should be at 
     */
    if (_debug) {
          click_chatter("%{element} short success for %s\n",
			this,
			dst.s().cc());
    }
    return;
  }

  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    if (_debug) {
          click_chatter("%{element} no info for %s\n",
			this,
			dst.s().cc());
    }
    return;
  }

  if (!success) {
    click_chatter("%{element} packet failed %s retries %d rate %d alt %d\n",
		  this,
		  dst.s().cc(),
		  retries,
		  ceh->rate,
		  ceh->alt_rate);
  }

  unsigned time = 0;
  unsigned alt_time = 0;
  if (_alt_rate && used_alt_rate) {
    alt_time = calc_usecs_wifi_packet_tries(1500, ceh->alt_rate, 
					     _original_retries, _original_retries + alt_retries);
  }
  time = calc_usecs_wifi_packet(1500, ceh->rate, retries);

  if (0) {
    click_chatter(" rate %d retries %d time %d alt_time %d alt_rate %d alt_retries %d\n",
		  ceh->rate, retries, time, alt_time,
		  ceh->alt_rate, alt_retries);
  }
  nfo->add_result(now, ceh->rate, success, time + alt_time);

  if (!success && used_alt_rate) {
    /* packet actually failed */
    nfo->add_result(now, ceh->alt_rate, false, alt_time);
  }
 
  //nfo->check();
  return ;
}

Packet *
ProbeTXRate::pull(int port)
{
  Packet *p = input(port).pull();
  if (p && _active) {
    assign_rate(p);
  }
  return p;
}

void
ProbeTXRate::push(int port, Packet *p_in)
{
  if (!p_in) {
    return;
  }
  if (port != 0) {
    if (_active) {
      process_feedback(p_in);
    }
    p_in->kill();
  } else {
    if (_active) {
      assign_rate(p_in);
    }
    output(port).push(p_in);
  }
}



String
ProbeTXRate::print_rates() 
{
  StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter; iter++) {
    DstInfo nfo = iter.value();
    sa << nfo._eth << "\n";
    for (int x = 0; x < nfo._rates.size(); x++) {
      sa << " " << nfo._rates[x];
      sa << " success " << nfo._total_success[x];
      sa << " fail " << nfo._total_fail[x];
      sa << " total_usecs " << nfo._total_time[x];
      sa << " perfect_usecs " << nfo._perfect_time[x];
      sa << " average_usecs ";
      if (nfo._total_success[x]) {
	int usecs = nfo._total_time[x] / nfo._total_success[x];
	sa << usecs;
      } else {
	sa << "*";
      }
      sa << "\n";
    }
  }
  return sa.take_string();
}


enum {H_DEBUG, H_RATES, H_THRESHOLD, H_RESET, 
      H_FILTER_LOW_RATES,
      H_FILTER_NEVER_SUCCESS,
      H_AGGRESSIVE_ALT_RATE,
      H_OFFSET,
      H_ACTIVE, 
      H_ALT_RATE,
      H_ORIGINAL_RETRIES,
     };


static String
ProbeTXRate_read_param(Element *e, void *thunk)
{
  ProbeTXRate *td = (ProbeTXRate *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_THRESHOLD:
    return String(td->_packet_size_threshold) + "\n";
  case H_OFFSET:
    return String(td->_offset) + "\n";
  case H_FILTER_LOW_RATES:
    return String(td->_filter_low_rates) + "\n";
  case H_FILTER_NEVER_SUCCESS:
    return String(td->_filter_never_success) + "\n";
  case H_AGGRESSIVE_ALT_RATE:
    return String(td->_aggressive_alt_rate) + "\n";
  case H_RATES: {
    return td->print_rates();
  }
  case H_ACTIVE: 
    return String(td->_active) + "\n";
  case H_ALT_RATE: 
    return String(td->_alt_rate) + "\n";
  case H_ORIGINAL_RETRIES: 
    return String(td->_original_retries) + "\n";
  default:
    return String();
  }
}
static int 
ProbeTXRate_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  ProbeTXRate *f = (ProbeTXRate *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_FILTER_LOW_RATES: {
    bool filter_low_rates;
    if (!cp_bool(s, &filter_low_rates)) 
      return errh->error("filter_low_rates parameter must be boolean");
    f->_filter_low_rates = filter_low_rates;
    break;
  }
  case H_FILTER_NEVER_SUCCESS: {
    bool filter_never_success;
    if (!cp_bool(s, &filter_never_success)) 
      return errh->error("filter_never_success parameter must be boolean");
    f->_filter_never_success = filter_never_success;
    break;
  }
  case H_AGGRESSIVE_ALT_RATE: {
    bool aggressive_alt_rate;
    if (!cp_bool(s, &aggressive_alt_rate)) 
      return errh->error("aggressive_alt_rate parameter must be boolean");
    f->_aggressive_alt_rate = aggressive_alt_rate;
    break;
  }
  case H_THRESHOLD: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("threshold parameter must be unsigned");
    f->_packet_size_threshold = m;
    break;
  }
  case H_ORIGINAL_RETRIES: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("original_retries parameter must be unsigned");
    f->_original_retries = m;
    break;
  }
  case H_OFFSET: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("offset parameter must be unsigned");
    f->_offset = m;
    break;
  }
  case H_RESET:
    f->_neighbors.clear();
    break;
 case H_ACTIVE: {
    bool active;
    if (!cp_bool(s, &active)) 
      return errh->error("active must be boolean");
    f->_active = active;
    break;
  }
 case H_ALT_RATE: {
    bool alt_rate;
    if (!cp_bool(s, &alt_rate)) 
      return errh->error("alt_rate must be boolean");
    f->_alt_rate = alt_rate;
    break;
  }
  }
  return 0;
}



void
ProbeTXRate::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", ProbeTXRate_read_param, (void *) H_DEBUG);
  add_read_handler("filter_low_rates", ProbeTXRate_read_param, (void *) H_FILTER_LOW_RATES);
  add_read_handler("filter_never_success", ProbeTXRate_read_param, (void *) H_FILTER_NEVER_SUCCESS);
  add_read_handler("aggressive_alt_rate", ProbeTXRate_read_param, (void *) H_AGGRESSIVE_ALT_RATE);
  add_read_handler("rates", ProbeTXRate_read_param, (void *) H_RATES);
  add_read_handler("threshold", ProbeTXRate_read_param, (void *) H_THRESHOLD);
  add_read_handler("offset", ProbeTXRate_read_param, (void *) H_OFFSET);
  add_read_handler("active", ProbeTXRate_read_param, (void *) H_ACTIVE);
  add_read_handler("alt_rate", ProbeTXRate_read_param, (void *) H_ALT_RATE);
  add_read_handler("original_retries", ProbeTXRate_read_param, (void *) H_ORIGINAL_RETRIES);

  add_write_handler("debug", ProbeTXRate_write_param, (void *) H_DEBUG);
  add_write_handler("filter_low_rates", ProbeTXRate_write_param, (void *) H_FILTER_LOW_RATES);
  add_write_handler("filter_never_success", ProbeTXRate_write_param, (void *) H_FILTER_NEVER_SUCCESS);
  add_write_handler("aggressive_alt_rate", ProbeTXRate_write_param, (void *) H_AGGRESSIVE_ALT_RATE);
  add_write_handler("threshold", ProbeTXRate_write_param, (void *) H_THRESHOLD);
  add_write_handler("offset", ProbeTXRate_write_param, (void *) H_OFFSET);
  add_write_handler("reset", ProbeTXRate_write_param, (void *) H_RESET);
  add_write_handler("active", ProbeTXRate_write_param, (void *) H_ACTIVE);
  add_write_handler("alt_rate", ProbeTXRate_write_param, (void *) H_ALT_RATE);
  add_write_handler("original_retries", ProbeTXRate_write_param, (void *) H_ORIGINAL_RETRIES);
  
}
// generate Vector template instance
#include <click/bighashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, ProbeTXRate::DstInfo>;
template class DEQueue<ProbeTXRate::tx_result>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(ProbeTXRate)

