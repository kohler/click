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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <elements/wifi/availablerates.hh>
#include "autoratefallback.hh"

CLICK_DECLS

AutoRateFallback::AutoRateFallback()
  : _stepup(10),
    _stepdown(1),
    _offset(0),
    _packet_size_threshold(0)
{

  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

AutoRateFallback::~AutoRateFallback()
{
}

int
AutoRateFallback::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _active = true;
  _adaptive_stepup = true;
  int ret = Args(conf, this, errh)
      .read("OFFSET", _offset)
      .read("ADAPTIVE_STEPUP", _adaptive_stepup)
      .read("STEPUP", _stepup)
      .read("STEPDOWN", _stepdown)
      .read("RT", ElementCastArg("AvailableRates"), _rtable)
      .read("THRESHOLD", _packet_size_threshold)
      .read("ACTIVE", _active)
      .complete();
  return ret;
}

void
AutoRateFallback::process_feedback(Packet *p_in)
{
  if (!p_in) {
    return;
  }
  uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
  EtherAddress dst = EtherAddress(dst_ptr);
  struct click_wifi_extra *eh = WIFI_EXTRA_ANNO(p_in);
  bool success = !(eh->flags & WIFI_EXTRA_TX_FAIL);
  bool used_alt_rate = (eh->flags & WIFI_EXTRA_TX_USED_ALT_RATE);
  int rate = eh->rate;

  if (dst.is_group()) {
    /* don't record info for bcast packets */
    return;
  }

  if (0 == rate) {
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


  if (nfo->_rates[nfo->_current_index] != rate) {
    return;
  }


  if (used_alt_rate || !success) {
    /* step down 1 or 2 rates */
    int down = eh->max_tries / 2;
    if (down > 0) {
      down--;
    }
    int next_index = WIFI_MAX(0, nfo->_current_index - down);
    if (_debug) {
      click_chatter("%p{element} stepping down for %s from %d to %d\n",
		    this,
		    nfo->_eth.unparse().c_str(),
		    nfo->_rates[nfo->_current_index],
		    nfo->_rates[next_index]);
    }

    if (nfo->_wentup && _adaptive_stepup) {
      /* backoff the stepup */
      nfo->_stepup *= 2;
      nfo->_wentup = false;
    } else {
      nfo->_stepup = _stepup;
    }

    nfo->_successes = 0;
    nfo->_current_index = next_index;

    return;
  }

  if (nfo->_wentup) {
    /* reset adaptive stepup on a success */
    nfo->_stepup = _stepup;
  }
  nfo->_wentup = false;

  if (eh->max_tries == 0) {
    nfo->_successes++;
  } else {
    nfo->_successes = 0;
  }

  if (nfo->_successes > nfo->_stepup &&

      nfo->_current_index != nfo->_rates.size() - 1) {
    if (_debug) {
      click_chatter("%p{element} steping up for %s from %d to %d\n",
		    this,
		    nfo->_eth.unparse().c_str(),
		    nfo->_rates[nfo->_current_index],
		    nfo->_rates[WIFI_MIN(nfo->_rates.size() - 1,
					 nfo->_current_index + 1)]);
    }
    nfo->_current_index = WIFI_MIN(nfo->_current_index + 1, nfo->_rates.size() - 1);
    nfo->_successes = 0;
    nfo->_wentup = true;
  }
  return;
}


void
AutoRateFallback::assign_rate(Packet *p_in)
{
  if (!p_in) {
    click_chatter("%p{element} ah, !p_in\n",
		  this);
    return;
  }

  uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
  EtherAddress dst = EtherAddress(dst_ptr);

  struct click_wifi_extra *eh = WIFI_EXTRA_ANNO(p_in);
  eh->magic = WIFI_EXTRA_MAGIC;

  if (dst.is_group()) {
    Vector<int> rates = _rtable->lookup(_bcast);
    if (rates.size()) {
      eh->rate = rates[0];
    } else {
      eh->rate = 2;
    }
    return;
  }
  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo || !nfo->_rates.size()) {
    _neighbors.insert(dst, DstInfo(dst));
    nfo = _neighbors.findp(dst);
    nfo->_rates = _rtable->lookup(dst);
    nfo->_successes = 0;
    nfo->_wentup = false;
    nfo->_stepup = _stepup;
    /* start at the highest rate */
    nfo->_current_index = nfo->_rates.size() - 1;
    if (_debug) {
      click_chatter("%p{element} initial rate for %s is %d\n",
		    this,
		    nfo->_eth.unparse().c_str(),
		    nfo->_rates[nfo->_current_index]);
    }
  }



  int ndx = nfo->_current_index;
  eh->rate = nfo->_rates[ndx];
  eh->rate1 = (ndx - 1 >= 0) ? nfo->_rates[WIFI_MAX(ndx - 1, 0)] : 0;
  eh->rate2 = (ndx - 2 >= 0) ? nfo->_rates[WIFI_MAX(ndx - 2, 0)] : 0;
  eh->rate3 = (ndx - 3 >= 0) ? nfo->_rates[WIFI_MAX(ndx - 3, 0)] : 0;

  eh->max_tries = 4;
  eh->max_tries1 = (ndx - 1 >= 0) ? 2 : 0;
  eh->max_tries2 = (ndx - 2 >= 0) ? 2 : 0;
  eh->max_tries3 = (ndx - 3 >= 0) ? 2 : 0;
  return;

}


Packet *
AutoRateFallback::pull(int port)
{
  Packet *p = input(port).pull();
  if (p && _active) {
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
  if (_active) {
    if (port != 0) {
      process_feedback(p_in);
    } else {
      assign_rate(p_in);
    }
  }
  checked_output_push(port, p_in);
}


String
AutoRateFallback::print_rates()
{
    StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter.live(); iter++) {
    DstInfo nfo = iter.value();
    sa << nfo._eth << " ";
    sa << nfo._rates[nfo._current_index] << " ";
    sa << nfo._successes << "\n";
  }
  return sa.take_string();
}


enum {H_DEBUG, H_STEPUP, H_STEPDOWN, H_THRESHOLD, H_RATES, H_RESET,
      H_OFFSET, H_ACTIVE};


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
  case H_OFFSET:
    return String(td->_offset) + "\n";
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
AutoRateFallback_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AutoRateFallback *f = (AutoRateFallback *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!BoolArg().parse(s, debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_STEPUP: {
    unsigned m;
    if (!IntArg().parse(s, m))
      return errh->error("stepup parameter must be unsigned");
    f->_stepup = m;
    break;
  }
  case H_STEPDOWN: {
    unsigned m;
    if (!IntArg().parse(s, m))
      return errh->error("stepdown parameter must be unsigned");
    f->_stepdown = m;
    break;
  }
  case H_THRESHOLD: {
    unsigned m;
    if (!IntArg().parse(s, m))
      return errh->error("threshold parameter must be unsigned");
    f->_packet_size_threshold = m;
    break;
  }
  case H_OFFSET: {
    unsigned m;
    if (!IntArg().parse(s, m))
      return errh->error("offset parameter must be unsigned");
    f->_offset = m;
    break;
  }
  case H_RESET:
    f->_neighbors.clear();
    break;
 case H_ACTIVE: {
    bool active;
    if (!BoolArg().parse(s, active))
      return errh->error("active must be boolean");
    f->_active = active;
    break;
  }
  }
  return 0;
}


void
AutoRateFallback::add_handlers()
{
  add_read_handler("debug", AutoRateFallback_read_param, H_DEBUG);
  add_read_handler("rates", AutoRateFallback_read_param, H_RATES);
  add_read_handler("threshold", AutoRateFallback_read_param, H_THRESHOLD);
  add_read_handler("stepup", AutoRateFallback_read_param, H_STEPUP);
  add_read_handler("stepdown", AutoRateFallback_read_param, H_STEPDOWN);
  add_read_handler("offset", AutoRateFallback_read_param, H_OFFSET);
  add_read_handler("active", AutoRateFallback_read_param, H_ACTIVE);

  add_write_handler("debug", AutoRateFallback_write_param, H_DEBUG);
  add_write_handler("threshold", AutoRateFallback_write_param, H_THRESHOLD);
  add_write_handler("stepup", AutoRateFallback_write_param, H_STEPUP);
  add_write_handler("stepdown", AutoRateFallback_write_param, H_STEPDOWN);
  add_write_handler("reset", AutoRateFallback_write_param, H_RESET, Handler::BUTTON);
  add_write_handler("offset", AutoRateFallback_write_param, H_OFFSET);
  add_write_handler("active", AutoRateFallback_write_param, H_ACTIVE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AutoRateFallback)

