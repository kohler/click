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

AutoTXRate::AutoTXRate()
  : Element(1, 1),
    _stepup(0),
    _stepdown(0),
    _before_switch(0),
    _max_rate(0)
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
  return new AutoTXRate();
}

int
AutoTXRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  unsigned int rate_window;
  int ret = cp_va_parse(conf, this, errh,
			cpKeywords, 
			"MAX_RATE", cpInteger, "int", &_max_rate,
			"RATE_WINDOW", cpUnsigned, "ms", &rate_window,
			"STEPUP", cpInteger, "0-100", &_stepup,
			"STEPDOWN", cpInteger, "0-100", &_stepdown,
			"BEFORE_SWITCH", cpInteger, "packets", &_before_switch,
			0);
  if (ret < 0) {
    return ret;
  }
  switch (_max_rate) {
  case 1:
    /* fallthrough */
  case 2:
    /* fallthrough */
  case 5:
    /* fallthrough */
  case 11:
    break;
  default:
    return errh->error("MAX_RATE must be 1,2,5, or 11");
  }
  ret = set_rate_window(errh, rate_window);
  if (ret < 0) {
    return ret;
  }
  return 0;
}
int 
AutoTXRate::next_lower_rate(int rate) {
    switch(rate) {
    case 1:
      return min(_max_rate, 1);
    case 2:
      return min(_max_rate, 1);
    case 5:
      return min(_max_rate, 2);
    case 11:
      return min(_max_rate, 5);
    default:
      return min(_max_rate, rate);
    }
  }


int 
AutoTXRate::next_higher_rate(int rate) {
    switch(rate) {
    case 1:
      return min(_max_rate, 2);
    case 2:
      return min(_max_rate, 5);
    case 5:
      return min(_max_rate, 11);
    case 11:
      return min(_max_rate, 11);
    default:
      return min(_max_rate, rate);
    }
  }
  



void
AutoTXRate::update_rate(EtherAddress dst)
{
  struct timeval now;
  struct timeval earliest;
  click_gettimeofday(&now);
  timersub(&now, &_rate_window, &earliest);

  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    return;
  }
  nfo->_rate = min(nfo->_rate, _max_rate);
  nfo->_successes = 0;
  nfo->_failures = 0;

  /* pop out all the old results */
  while (nfo->_results.size() > 0 && 
	 timercmp(&earliest, &nfo->_results[0]._when, >)) {
    nfo->_results.pop_front();
  }
  
  /* count the resluts in the last _rate_window for which
   * were sent at nfo->_rate
   */
  for (int i = 0; i < nfo->_results.size(); i++) {
    if (nfo->_results[i]._rate == nfo->_rate) {
      if (nfo->_results[i]._success) {
	nfo->_successes++;
      } else {
	nfo->_failures++;
      }
    }
  }
  
  int total = nfo->_successes + nfo->_failures;
  
  if (!total || total < _before_switch) {
    return;
  }
  if ((100*nfo->_successes)/total < _stepdown) {
    nfo->_rate = next_lower_rate(nfo->_rate);
  } else if ((100*nfo->_successes)/total > _stepup) {
    nfo->_rate = next_higher_rate(nfo->_rate);
  }

  return;
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
  struct timeval now;
  click_gettimeofday(&now);

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
    _neighbors.insert(dst, DstInfo(dst, rate));
    nfo = _neighbors.findp(dst);
  }
  nfo->_results.push_back(tx_result(now, rate, success));
  update_rate(dst);
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
  typedef BigHashMap<EtherAddress, bool> EthMap;
  EthMap ethers;

  for (NIter iter = _neighbors.begin(); iter; iter++) {
    ethers.insert(iter.key(), true);
  }

  struct timeval now;
  click_gettimeofday(&now);

  
  StringAccum sa;
  for (EthMap::const_iterator i = ethers.begin(); i; i++) {
    update_rate(i.key());
    DstInfo *n = _neighbors.findp(i.key());
    sa << n->_eth.s().cc();
    sa << " rate " << n->_rate;
    sa << " successes " << n->_successes;
    sa << " failures " << n->_failures;
    sa << " percent ";
    int total = n->_successes + n->_failures;
    if (!total || total  <  _before_switch) {
      sa << "xxx";
    } else {
      sa << (n->_successes*100) / total;
    }
    sa << "\n";
  }
  return sa.take_string();
}


int
AutoTXRate::static_write_rate_window(const String &arg, Element *e,
				     void *, ErrorHandler *errh) 
{
  AutoTXRate *n = (AutoTXRate *) e;
  unsigned int b;

  if (!cp_unsigned(arg, &b))
    return errh->error("`rate_window' must be a unsigned int");

  return n->set_rate_window(errh, b);
}

int
AutoTXRate::static_write_before_switch(const String &arg, Element *e,
				     void *, ErrorHandler *errh) 
{
  AutoTXRate *n = (AutoTXRate *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`before_switch' must be an integer");

  n->_before_switch = b;
  return 0;
}


int
AutoTXRate::static_write_stepdown(const String &arg, Element *e,
				     void *, ErrorHandler *errh) 
{
  AutoTXRate *n = (AutoTXRate *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`stepdown' must be an integer");

  n->_stepdown = b;
  return 0;
}



int
AutoTXRate::static_write_stepup(const String &arg, Element *e,
				     void *, ErrorHandler *errh) 
{
  AutoTXRate *n = (AutoTXRate *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`stepup' must be an integer");
  
  n->_stepup = b;
  return 0;
}



int
AutoTXRate::static_write_max_rate(const String &arg, Element *e,
				  void *, ErrorHandler *errh) 
{
  AutoTXRate *n = (AutoTXRate *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`stepup' must be an integer");
  
  switch (b) {
  case 1:
    /* fallthrough */
  case 2:
    /* fallthrough */
  case 5:
    /* fallthrough */
  case 11:
    break;
  default:
    return errh->error("MAX_RATE must be 1,2,5, or 11");
  }
  n->_max_rate = b;
  return 0;
}




int
AutoTXRate::set_rate_window(ErrorHandler *errh, unsigned int x) 
{

  if (!x) {
    return errh->error("RATE_WINDOW must not be 0");
  }
  timerclear(&_rate_window);
  /* convehop path_duration from ms to a struct timeval */
  _rate_window.tv_sec = x/1000;
  _rate_window.tv_usec = (x % 1000) * 1000;
  return 0;
}

String
AutoTXRate::static_read_rate_window(Element *f, void *)
{
  StringAccum sa;
  AutoTXRate *d = (AutoTXRate *) f;
  sa << d->_rate_window << "\n";
  return sa.take_string();
}


String
AutoTXRate::static_read_stepup(Element *f, void *)
{
  StringAccum sa;
  AutoTXRate *d = (AutoTXRate *) f;
  sa << d->_stepup << "\n";
  return sa.take_string();
}

String
AutoTXRate::static_read_stepdown(Element *f, void *)
{
  StringAccum sa;
  AutoTXRate *d = (AutoTXRate *) f;
  sa << d->_stepdown << "\n";
  return sa.take_string();
}


String
AutoTXRate::static_read_before_switch(Element *f, void *)
{
  StringAccum sa;
  AutoTXRate *d = (AutoTXRate *) f;
  sa << d->_before_switch << "\n";
  return sa.take_string();
}

String
AutoTXRate::static_read_max_rate(Element *f, void *)
{
  StringAccum sa;
  AutoTXRate *d = (AutoTXRate *) f;
  sa << d->_max_rate << "\n";
  return sa.take_string();
}

void
AutoTXRate::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("stats", static_print_stats, 0);

  add_write_handler("rate_window", static_write_rate_window, 0);
  add_read_handler("rate_window", static_read_rate_window, 0);

  add_write_handler("stepup", static_write_stepup, 0);
  add_read_handler("stepup", static_read_stepup, 0);

  add_write_handler("stepdown", static_write_stepdown, 0);
  add_read_handler("stepdown", static_read_stepdown, 0);

  add_write_handler("before_switch", static_write_before_switch, 0);
  add_read_handler("before_switch", static_read_before_switch, 0);

  add_write_handler("max_rate", static_write_max_rate, 0);
  add_read_handler("max_rate", static_read_max_rate, 0);

}
// generate Vector template instance
#include <click/bighashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<EtherAddress, AutoTXRate::DstInfo>;
template class DEQueue<AutoTXRate::tx_result>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(AutoTXRate)

