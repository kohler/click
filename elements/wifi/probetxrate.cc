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

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

ProbeTXRate::ProbeTXRate()
  : Element(2, 1),
    _rate_window_ms(0),
    _rtable(0),
    _packet_size_threshold(0)
{
  MOD_INC_USE_COUNT;

  /* bleh */
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
  unsigned int rate_window;
  int ret = cp_va_parse(conf, this, errh,
			cpKeywords, 
			"WINDOW", cpUnsigned, "window", &_rate_window_ms,
			"THRESHOLD", cpUnsigned, "window", &_packet_size_threshold,
			"ETH", cpEthernetAddress, "eth", &_eth,
			"RT", cpElement, "availablerates", &_rtable,
			0);
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



Packet *
ProbeTXRate::pull(int port)
{
  return assign_rate(input(port).pull());
}
Packet *
ProbeTXRate::assign_rate(Packet *p_in) {
  
  if (!p_in) {
    return 0;
  }
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  SET_WIFI_FROM_CLICK(p_in);

  if (dst == _bcast) {
    SET_WIFI_RATE_ANNO(p_in, 2);
    return p_in;
  }
  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    _neighbors.insert(dst, DstInfo(dst));
    nfo = _neighbors.findp(dst);
    nfo->_rates = _rtable->lookup(dst);
    nfo->_total_tries = Vector<int>(nfo->_rates.size(), 0);
    nfo->_total_success = Vector<int>(nfo->_rates.size(), 0);
  }

  struct timeval now;
  struct timeval old;
  click_gettimeofday(&now);
  timersub(&now, &_rate_window, &old);
  nfo->trim(old);
  int rate = nfo->pick_rate();
  SET_WIFI_RATE_ANNO(p_in, rate);
  SET_WIFI_ALT_RATE_ANNO(p_in, 2);
  SET_WIFI_ALT_RETRIES_ANNO(p_in, 4);
  return p_in;
}

void
ProbeTXRate::push(int port, Packet *p_in)
{
  if (!p_in) {
    return;
  }
  if (port != 0) {
    process_feedback(p_in);
    p_in->kill();
  } else {
    output(port).push(assign_rate(p_in));
  }
}
void
ProbeTXRate::process_feedback(Packet *p_in) {

  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  int success = (WIFI_TX_STATUS_ANNO(p_in) == 0);  
  int retries = WIFI_RETRIES_ANNO(p_in);
  int rate = WIFI_RATE_ANNO(p_in);

  struct timeval now;
  click_gettimeofday(&now);

  if (dst == _bcast) {
    /* don't record info for bcast packets */
    click_chatter("%{element}: discarding bcast %s\n",
		  this,
		  dst.s().cc());
    return;
  }

  if (0 == rate) {
    /* rate wasn't set */
    return;
  }
  if (success && p_in->length() < _packet_size_threshold) {
    /* 
     * don't deal with short packets, 
     * since they can skew what rate
     * we should be at 
     */
    return;
  }

  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    return;
  }


  nfo->add_result(now, rate, retries + 1, success);

  return ;
}



String
ProbeTXRate::print_rates() 
{
  StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter; iter++) {
    DstInfo nfo = iter.value();
    sa << nfo._eth << "\n";
    for (int x = 0; x < nfo._rates.size(); x++) {
      sa << " " << nfo._rates[x] << " " << nfo._total_tries[x] << " " << nfo._total_success[x] << " ";
      if (nfo._total_success[x]) {
	int tput = rate_to_tput(nfo._rates[x]) * nfo._total_tries[x] / nfo._total_success[x];
	sa << tput;
      } else {
	sa << "*";
      }
      sa << "\n";
    }
  }
  return sa.take_string();
}
enum {H_DEBUG, H_ETH, H_RATES, H_THRESHOLD};


static String
ProbeTXRate_read_param(Element *e, void *thunk)
{
  ProbeTXRate *td = (ProbeTXRate *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_ETH:
    return td->_eth.s() + "\n";
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
  case H_THRESHOLD: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("threshold parameter must be unsigned");
    f->_packet_size_threshold = m;
    break;
  }
  case H_ETH: {
    EtherAddress e;
    if (!cp_ethernet_address(s, &e)) 
      return errh->error("bssid parameter must be ethernet address");
    f->_eth = e;
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
  add_read_handler("eth", ProbeTXRate_read_param, (void *) H_ETH);
  add_read_handler("rates", ProbeTXRate_read_param, (void *) H_RATES);
  add_read_handler("threshold", ProbeTXRate_read_param, (void *) H_THRESHOLD);

  add_write_handler("debug", ProbeTXRate_write_param, (void *) H_DEBUG);
  add_write_handler("eth", ProbeTXRate_write_param, (void *) H_ETH);
  add_write_handler("threshold", ProbeTXRate_write_param, (void *) H_THRESHOLD);
  
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

