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
#include <click/args.hh>
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
  : _offset(0),
    _packet_size_threshold(0),
    _rate_window_ms(0),
    _rtable(0)
{

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

ProbeTXRate::~ProbeTXRate()
{
}

int
ProbeTXRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _debug = false;
  _active = true;
  _original_retries = 4;
  _min_sample = 20;
  int ret = Args(conf, this, errh)
      .read("OFFSET", _offset)
      .read("WINDOW", _rate_window_ms)
      .read("THRESHOLD", _packet_size_threshold)
      .read("DEBUG", _debug)
      .read_m("RT", ElementCastArg("AvailableRates"), _rtable)
      .read("ACTIVE", _active)
      .complete();
  if (ret < 0) {
    return ret;
  }
  if (_rate_window_ms <= 0)
    return errh->error("WINDOW must be > 0");

  _rate_window = Timestamp::make_msec(_rate_window_ms);

  return ret;
}




void
ProbeTXRate::assign_rate(Packet *p_in) {

  if (!p_in) {
    click_chatter("%p{element} ah, !p_in\n", this);
    return;
  }

  uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
  EtherAddress dst = EtherAddress(dst_ptr);
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);

  if (dst.is_group() || !dst) {
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

  nfo->trim(Timestamp::now() - _rate_window);
  //nfo->check();


  int best_ndx = nfo->best_rate_ndx();
  if (nfo->_count++ % 10) {
    // pick the best bit-rate
    ceh->rate = nfo->_rates[best_ndx];
    ceh->max_tries = 4;
  } else {
    //pick a random rate.
    Vector<int> possible = nfo->pick_rate();
    if (possible.size()) {
      ceh->rate = possible[click_random(0, possible.size() - 1)];
      ceh->max_tries = 2;
    } else {
      // no rates to sample from
      ceh->rate = nfo->_rates[best_ndx];
      ceh->max_tries = 4;
    }
  }


  ceh->rate1 = nfo->_rates[best_ndx];
  ceh->max_tries1 = 2;

  ceh->rate2 = 0;
  ceh->max_tries2 = 0;

  ceh->rate3 = 0;
  ceh->max_tries3 = 0;

  return;
}


void
ProbeTXRate::process_feedback(Packet *p_in) {

  if (!p_in) {
    return;
  }
  uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
  EtherAddress dst = EtherAddress(dst_ptr);
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
  bool success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);
  int retries = ceh->max_tries;

  Timestamp now = Timestamp::now();

  if (dst == _bcast) {
    /* don't record info for bcast packets */
    if (_debug) {
      click_chatter("%p{element}: discarding bcast %s\n",
		    this,
		    dst.unparse().c_str());
    }
    return;
  }

  if (0 == ceh->rate) {
    /* rate wasn't set */
    if (_debug) {
          click_chatter("%p{element} no rate set for %s\n",
			this,
			dst.unparse().c_str());
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
          click_chatter("%p{element} short success for %s\n",
			this,
			dst.unparse().c_str());
    }
    return;
  }

  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    if (_debug) {
          click_chatter("%p{element} no info for %s\n",
			this,
			dst.unparse().c_str());
    }
    return;
  }

  if (!success && _debug) {
    click_chatter("%p{element} packet failed %s retries %d rate %d alt %d\n",
		  this,
		  dst.unparse().c_str(),
		  retries,
		  ceh->rate,
		  ceh->rate1);
  }


  int tries = retries+1;
  int time = calc_usecs_wifi_packet(1500, ceh->rate,
				    retries);

  if (_debug) {
	  click_chatter("%p{element}::%s() rate %d tries %d (retries %d) time %d\n",
			this, __func__, ceh->rate, tries, retries, time);
  }
  nfo->add_result(now, ceh->rate, tries,
		  success, time);
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
ProbeTXRate::print_rates()
{
  StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter.live(); iter++) {
    DstInfo nfo = iter.value();
    sa << nfo._eth << "\n";
    for (int x = 0; x < nfo._rates.size(); x++) {
	sa << " " << nfo._rates[x];
	sa << " success " << nfo._total_success[x];
	sa << " fail " << nfo._total_fail[x];
	sa << " tries " << nfo._total_tries[x];
	sa << " perfect_usecs " << nfo._perfect_time[x];
	sa << " total_usecs " << nfo._total_time[x];
	sa << " average_usecs " << nfo.average(x);

	sa << " average_tries ";

	if (nfo._packets[x]) {
	  Timestamp average_tries = Timestamp::make_msec(nfo._total_tries[x] * 1000 / nfo._packets[x]);
	  sa << average_tries;
	} else {
	  sa << "0";
	}


	sa << "\n";
    }
  }
  return sa.take_string();
}


enum {H_DEBUG,
      H_RATES,
      H_THRESHOLD,
      H_RESET,
      H_OFFSET,
      H_ACTIVE,
     };


static String
ProbeTXRate_read_param(Element *e, void *thunk)
{
  ProbeTXRate *td = (ProbeTXRate *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:     return String(td->_debug) + "\n";
  case H_THRESHOLD: return String(td->_packet_size_threshold) + "\n";
  case H_OFFSET:    return String(td->_offset) + "\n";
  case H_RATES:     return td->print_rates();
  case H_ACTIVE:    return String(td->_active) + "\n";
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
  switch((intptr_t)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!BoolArg().parse(s, debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
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
ProbeTXRate::add_handlers()
{
  add_read_handler("debug", ProbeTXRate_read_param, H_DEBUG);
  add_read_handler("rates", ProbeTXRate_read_param, H_RATES);
  add_read_handler("threshold", ProbeTXRate_read_param, H_THRESHOLD);
  add_read_handler("offset", ProbeTXRate_read_param, H_OFFSET);
  add_read_handler("active", ProbeTXRate_read_param, H_ACTIVE);

  add_write_handler("debug", ProbeTXRate_write_param, H_DEBUG);
  add_write_handler("threshold", ProbeTXRate_write_param, H_THRESHOLD);
  add_write_handler("offset", ProbeTXRate_write_param, H_OFFSET);
  add_write_handler("reset", ProbeTXRate_write_param, H_RESET, Handler::BUTTON);
  add_write_handler("active", ProbeTXRate_write_param, H_ACTIVE);

}

CLICK_ENDDECLS
ELEMENT_REQUIRES(bitrate)
EXPORT_ELEMENT(ProbeTXRate)


