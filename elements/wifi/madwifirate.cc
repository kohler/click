/*
 * madwifirate.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include <clicknet/wifi.h>
#include <elements/wifi/availablerates.hh>
#include "madwifirate.hh"

CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

MadwifiRate::MadwifiRate()
  : Element(2, 1),
    _stepup(0),
    _stepdown(0),
    _offset(0), 
    _timer(this),
    _packet_size_threshold(0)
{
  MOD_INC_USE_COUNT;

  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}


void
MadwifiRate::run_timer()
{
  _timer.schedule_after_ms(1000);
}
MadwifiRate::~MadwifiRate()
{
  MOD_DEC_USE_COUNT;
}

int 
MadwifiRate::initialize(ErrorHandler *) 
{
  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}
int
MadwifiRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _alt_rate = false;
  _active = true;
  int ret = cp_va_parse(conf, this, errh,
			cpKeywords, 
			"OFFSET", cpUnsigned, "offset", &_offset,
			"RT", cpElement, "availablerates", &_rtable,
			"THRESHOLD", cpUnsigned, "xxx", &_packet_size_threshold,
			"ALT_RATE", cpBool, "xxx", &_alt_rate,
			"ACTIVE", cpBool, "xxx", &_active,
			0);
  return ret;
}

void 
MadwifiRate::adjust_all()
{
  Vector<EtherAddress> n;
  for (NIter iter = _neighbors.begin(); iter; iter++) {
    DstInfo nfo = iter.value();
    n.push_back(nfo._eth);
  }

  for (int x =0; x < n.size(); x++) {
    adjust(n[x]);
  }

}

void 
MadwifiRate::adjust(EtherAddress dst) 
{
  DstInfo *nfo = _neighbors.findp(dst);
    bool stepup = false;
  bool stepdown = false;
  if (nfo->_failures > 0 && nfo->_successes == 0) {
    stepdown = true;
  }

  bool enough = (nfo->_successes + nfo->_failures) > 10;

  /* all packets need retry in average */
  if (enough && nfo->_successes < nfo->_retries)
    stepdown = true;

  /* no error and less than 10% of packets need retry */
  if (enough && nfo->_failures == 0 && nfo->_successes > nfo->_retries * 10)
    stepup = true;

  if (stepdown) {
    if (nfo->_rates.size()) {
      if (_debug && max(nfo->_current_index - 1, 0) != nfo->_current_index) {
	click_chatter("%{element} stepping down for %s from %d to %d\n",
		      this,
		      nfo->_eth.s().cc(),
		      nfo->_rates[nfo->_current_index],
		      nfo->_rates[max(0, nfo->_current_index - 1)]);
      }
    }
    nfo->_current_index = max(nfo->_current_index - 1, 0);
    nfo->_successes = 0;
    nfo->_failures = 0;
    nfo->_retries = 0;
  } else if (stepup) {
    if (nfo->_current_index == nfo->_rates.size() - 1) {
      return;
    }
    if (nfo->_rates.size()) {
      if (_debug) {
	click_chatter("%{element} steping up for %s from %d to %d\n",
		      this,
		      nfo->_eth.s().cc(),
		      nfo->_rates[nfo->_current_index],
		      nfo->_rates[min(nfo->_rates.size() - 1, 
				      nfo->_current_index + 1)]);
      }
    }
    nfo->_current_index = min(nfo->_current_index + 1, nfo->_rates.size() - 1);
    nfo->_successes = 0;
    nfo->_failures = 0;
    nfo->_retries = 0;
  }
  



}

void
MadwifiRate::process_feedback(Packet *p_in)
{
  if (!p_in) {
    click_chatter("%{element} bad packet %s\n",
		  this,
		  __func__);
    return;
  }
  uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
  EtherAddress dst = EtherAddress(dst_ptr);

  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p_in->all_user_anno();
  bool success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);
  bool used_alt_rate = ceh->flags & WIFI_EXTRA_TX_USED_ALT_RATE;

  struct timeval now;
  click_gettimeofday(&now);

  if (dst.is_group()) {
    /* don't record info for bcast packets */
    return;
  }

  if (0 == ceh->rate) {
    /* rate wasn't set */
    return;
  }
  if (success && p_in->length() < _packet_size_threshold) {
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


  if (nfo->pick_rate() != ceh->rate) {
    return;
  }

  if (!success && _debug) {
    click_chatter("%{element} packet failed %s success %d rate %d alt %d\n",
		  this,
		  dst.s().cc(),
		  success,
		  ceh->rate,
		  ceh->alt_rate
		  );
  }


  if (success && (!_alt_rate || !used_alt_rate)) {
    nfo->_successes++;
    nfo->_retries += ceh->retries;
  } else {
    nfo->_failures++;
    nfo->_retries += 4;
  }

  return;
}


void
MadwifiRate::assign_rate(Packet *p_in)
{
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
    _neighbors.insert(dst, DstInfo(dst));
    nfo = _neighbors.findp(dst);
    nfo->_rates = _rtable->lookup(dst);
    nfo->_successes = 0;
    nfo->_retries = 0;
    nfo->_failures = 0;
    /* initial to 24 in g/a, 11 in b */
    int ndx = nfo->rate_index(48);
    ndx = ndx > 0 ? ndx : nfo->rate_index(22);
    ndx = max(ndx, 0);
    nfo->_current_index = ndx;
    if (_debug) {
      click_chatter("%{element} initial rate for %s is %d\n",
		    this,
		    nfo->_eth.s().cc(),
		    nfo->_rates[nfo->_current_index]);
    }
  }

  ceh->rate = nfo->pick_rate();
  ceh->max_retries = (_alt_rate) ? 3 : WIFI_MAX_RETRIES;
  ceh->alt_rate = (_alt_rate) ? nfo->pick_alt_rate() : 0;
  ceh->alt_max_retries = (_alt_rate) ? WIFI_MAX_RETRIES - 3 : 0;
  return;
  
}


Packet *
MadwifiRate::pull(int port)
{
  Packet *p = input(port).pull();
  if (p && _active) {
    assign_rate(p);
  }
  return p;
}

void
MadwifiRate::push(int port, Packet *p_in)
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
MadwifiRate::print_rates() 
{
    StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter; iter++) {
    DstInfo nfo = iter.value();
    sa << nfo._eth << " ";
    if (nfo._rates.size()) {
      sa << nfo._rates[nfo._current_index];
      sa << " successes " << nfo._successes;
      sa << " failures " << nfo._failures;
      sa << " retries " << nfo._retries;
    }
    sa << "\n";
  }
  return sa.take_string();
}


enum {H_DEBUG, H_STEPUP, H_STEPDOWN, H_THRESHOLD, H_RATES, H_RESET, 
      H_OFFSET, H_ACTIVE,
      H_ALT_RATE};


static String
MadwifiRate_read_param(Element *e, void *thunk)
{
  MadwifiRate *td = (MadwifiRate *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_STEPDOWN:
    return String(td->_stepdown) + "\n";
  case H_STEPUP:
    return String(td->_stepup) + "\n";
  case H_THRESHOLD:
    return String(td->_packet_size_threshold) + "\n";
  case H_OFFSET:
    return String(td->_offset) + "\n";
  case H_ALT_RATE:
    return String(td->_alt_rate) + "\n";
  case H_RATES: {
    return td->print_rates();
  }
  case H_ACTIVE: 
    return String(td->_active) + "\n";
  default:
    return String();
  }
}
static int 
MadwifiRate_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  MadwifiRate *f = (MadwifiRate *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_ALT_RATE: {
    bool alt_rate;
    if (!cp_bool(s, &alt_rate)) 
      return errh->error("alt_rate parameter must be boolean");
    f->_alt_rate = alt_rate;
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
  }
  return 0;
}


void
MadwifiRate::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", MadwifiRate_read_param, (void *) H_DEBUG);
  add_read_handler("rates", MadwifiRate_read_param, (void *) H_RATES);
  add_read_handler("threshold", MadwifiRate_read_param, (void *) H_THRESHOLD);
  add_read_handler("stepup", MadwifiRate_read_param, (void *) H_STEPUP);
  add_read_handler("stepdown", MadwifiRate_read_param, (void *) H_STEPDOWN);
  add_read_handler("offset", MadwifiRate_read_param, (void *) H_OFFSET);
  add_read_handler("active", MadwifiRate_read_param, (void *) H_ACTIVE);
  add_read_handler("alt_rate", MadwifiRate_read_param, (void *) H_ALT_RATE);

  add_write_handler("debug", MadwifiRate_write_param, (void *) H_DEBUG);
  add_write_handler("threshold", MadwifiRate_write_param, (void *) H_THRESHOLD);
  add_write_handler("stepup", MadwifiRate_write_param, (void *) H_STEPUP);
  add_write_handler("stepdown", MadwifiRate_write_param, (void *) H_STEPDOWN);
  add_write_handler("offset", MadwifiRate_write_param, (void *) H_OFFSET);
  add_write_handler("reset", MadwifiRate_write_param, (void *) H_RESET);
  add_write_handler("active", MadwifiRate_write_param, (void *) H_ACTIVE);
  add_write_handler("alt_rate", MadwifiRate_write_param, (void *) H_ALT_RATE);

}
// generate Vector template instance
#include <click/bighashmap.cc>
#include <click/dequeue.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, MadwifiRate::DstInfo>;
template class DEQueue<MadwifiRate::tx_result>;
template class Vector<EtherAddress>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(MadwifiRate)

