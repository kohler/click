/*
 * txfeedbackstats.{cc,hh} -- track per-link transmission statistics.
 * Douglas S. J. De Couto
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
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <elements/wifi/txfeedbackstats.hh>
#include <elements/grid/timeutils.hh>
CLICK_DECLS

TXFeedbackStats::TXFeedbackStats()
  : _tau(10000), _min_pkts(10)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

TXFeedbackStats::~TXFeedbackStats()
{
  MOD_DEC_USE_COUNT;
}

int
TXFeedbackStats::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"TAU", cpUnsigned, "TX feedback tracking period (msecs)", &_tau,
			"MIN_PKTS", cpUnsigned, "minimum number of packets required to estimate TX count", &_min_pkts,
			cpEnd);
  if (res < 0)
    return res;

  if (_min_pkts < 1)
    return errh->error("MIN_PKTS must be >= 1");
  return 0;
}

int
TXFeedbackStats::initialize(ErrorHandler *)
{
  _tau_tv.tv_sec = _tau / 1000;
  _tau_tv.tv_usec = 1000 * (_tau % 1000);
  return 0;
}


Packet *
TXFeedbackStats::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();

  if (p->length() < sizeof(click_ether)) {
    click_chatter("TXFeedbackStats %s: got short packet, expected at least %u, but only got %u", 
		  id().cc(), p->length());
    return p;
  }  

  // decoding values from aironet tx fid, see manual p. 7-46
  /* eventually this card-specific stuff will be moved into a
     FooTxFeedback el, once we sort out what the annos look like */
  tx_result_t res;
  switch (p->user_anno_c(0)) {
  case 0: res = TxOk; break;
  case 2: res = TxLifetimeExceeded; break;
  case 4: res = TxMaxRetriesExceed; break;
  default: res = TxUnknownResult;
  }

  unsigned n_data = p->user_anno_c(3);
  unsigned n_rts = p->user_anno_c(2);

  if (res == TxOk)
    n_data++;
  else if (n_data > 0) {
    // we failed, but we know we were trying to make some data
    // transmissions, not just waiting on RTS
    n_data++; 
  }
  
  // if we know we did at least some RTS, add one to number of
  // retries.  we won't know if we were using RTS but the first RTS
  // always succeeds.
  if (n_rts > 0)
    n_rts++;

  add_stat(EtherAddress(eh->ether_dhost), p->length(), p->timestamp_anno(), res, n_data, n_rts);

  return p;
}

void
TXFeedbackStats::add_stat(const EtherAddress &dest, int sz, const timeval &when, 
			  tx_result_t res, unsigned data_attempts, unsigned  rts_attempts)
{
  StatQ *q = cleanup_map(dest);
  if (!q) {
    _stat_map.insert(dest, StatQ());
    q = _stat_map.findp(dest);
  }

  q->push_front(stat_t(when, sz, res, data_attempts, rts_attempts));
}

TXFeedbackStats::StatQ *
TXFeedbackStats::cleanup_map(const EtherAddress &dest)
{
  struct timeval oldest;
  click_gettimeofday(&oldest);
  oldest -= _tau_tv;


  StatQ *q = _stat_map.findp(dest);
  if (!q)
    return 0;

  // discard too-old stats
  while (q->size() > 0 && q->back().when < oldest)
    q->pop_back();

  // could also remove StatQs for dests with no data.  for now we'll
  // pretend that there are only a limited number of neighbors we'll
  // ever see.
  
  return q;
}

bool
TXFeedbackStats::est_tx_count(const EtherAddress &dest, unsigned &etx)
{
  unsigned n_data, n_rts, n_pkts;
  if (!get_counts(dest, n_data, n_rts, n_pkts))
    return false;

  if (n_pkts < _min_pkts)
    return false;    

  etx = 100 * (n_rts + n_data) / n_pkts;

  return true;
}

bool
TXFeedbackStats::get_counts(const EtherAddress &dest, unsigned &n_data, unsigned &n_rts, unsigned &n_pkts)
{
  StatQ *q = cleanup_map(dest);
  if (!q)
    return false;

  n_pkts = (unsigned) q->size();
  n_data = 0;
  n_rts = 0;
  for (StatQ::const_iterator i = q->begin(); i != q->end(); i++) {
    n_data += i->n_data;
    n_rts += i->n_rts;
  }
  return true;
}

String
TXFeedbackStats::print_stats()
{
  Vector<EtherAddress> v;
  for (StatMap::const_iterator i = _stat_map.begin(); i; i++)
    v.push_back(i.key());

  StringAccum sa;

  for (int i = 0; i < v.size(); i++) {
    sa << v[i] << "  ";
    unsigned n_data, n_rts, n_pkts;
    if (!get_counts(v[i], n_data, n_rts, n_pkts)) 
      sa << "No data available";
    else {
      sa << n_data << "\t" << n_rts << "\t" << n_pkts << "\t";
      if (n_pkts < _min_pkts)
	sa << "Too few packets";
      else
	sa << (100 * (n_data + n_rts) / n_pkts);
    }
    sa << "\n";
  }
  return sa.take_string();  
}

String
TXFeedbackStats::read_params(Element *xf, void *n)
{
  TXFeedbackStats *f = (TXFeedbackStats *) xf;
  switch ((int) n) {
  case 0: return String(f->_tau) + "\n";
  case 1: return String(f->_min_pkts) + "\n";
  case 2: return f->print_stats();
  default:
  return "<unknown parameter>";
  }

}

void
TXFeedbackStats::add_handlers()
{
  add_default_handlers(false);
  add_read_handler("tau", read_params, (void *)0);
  add_read_handler("min_pkts", read_params, (void *)1);
  add_read_handler("stats", read_params, (void *)2);
}

EXPORT_ELEMENT(TXFeedbackStats)
ELEMENT_REQUIRES(false)
#include <click/dequeue.cc>
template class DEQueue<TXFeedbackStats::stat_t>;

#include <click/bighashmap.cc>
template class HashMap<EtherAddress, TXFeedbackStats::StatQ>;

CLICK_ENDDECLS
