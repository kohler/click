/*
 * linkstat.{cc,hh} -- extract per-packet link quality/strength stats
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2002 Massachusetts Institute of Technology
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
#include "linkstat.hh"
#include <click/glue.hh>
#include <sys/time.h>
#include "grid.hh"
#include "timeutils.hh"
CLICK_DECLS

LinkStat::LinkStat()
  : _ai(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

LinkStat::~LinkStat()
{
  MOD_DEC_USE_COUNT;
}

LinkStat *
LinkStat::clone() const
{
  return new LinkStat();
}


int
LinkStat::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpElement, "AiroInfo element", &_ai,
			cpUnsigned, "Broadcast loss rate window (msecs)", &_window,
			0);
  if (res < 0)
    return res;
  
  return res;
}



int
LinkStat::initialize(ErrorHandler *)
{
  return 0;
}



Packet *
LinkStat::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  EtherAddress ea(eh->ether_shost);

  stat_t s;
  gettimeofday(&s.when, 0);

  bool res = _ai->get_signal_info(ea, s.sig, s.qual);
  if (res)
    _stats.insert(ea, s);

  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("LinkStat: got non-Grid packet type");
    return p;
  }

  grid_hdr *gh = (grid_hdr *) (eh + 1);
  if (gh->type != grid_hdr::GRID_LR_HELLO)
    return p;

  // grid_hello *hlo = (grid_hello *) (gh + 1);
  add_bcast_stat(ea, ntohl(grid_hdr::get_pad_bytes(*gh)));

  return p;
}

void
LinkStat::remove_all_stats(const EtherAddress &e)
{
  _stats.remove(e);
  /* don't want to remove bcast stats: window may be > expire timer */
  // _bcast_stats.remove(e);

}

bool
LinkStat::get_bcast_stats(const EtherAddress &e, struct timeval &last, unsigned int &window,
			  unsigned int &num_rx, unsigned int &num_expected)
{
  Vector<bcast_t> *v = _bcast_stats.findp(e);
  if (v == 0 || v->size() == 0)
    return false;

  unsigned int first_seq, last_seq;
  first_seq = v->at(0).seq;
  last_seq = v->back().seq;

  num_expected = 1 + (last_seq - first_seq);
  num_rx = v->size();
  last = v->back().when;
  
  window = _window;

  return true;
}

void
LinkStat::add_bcast_stat(const EtherAddress &e, unsigned int seqno)
{
  struct timeval now;
  gettimeofday(&now, 0);
  bcast_t s = { now, seqno };
  
  Vector<bcast_t> *v = _bcast_stats.findp(e);
  if (!v) {
    Vector<bcast_t> v2;
    _bcast_stats.insert(e, v2);
    v = _bcast_stats.findp(e);
  }
  
  v->push_back(s);
  
  /* only keep stats for last _window msecs */
  struct timeval delta;
  delta.tv_sec = _window / 1000;
  delta.tv_usec = (_window % 1000) * 1000;
  struct timeval last = now - delta;
      
  /* oh this is nice an efficient -- not.  too bad eddie's vector
     doesn't have queue ops.  fuck it. */
  Vector<bcast_t> new_vec;
  for (int i = 0; i < v->size(); i++) 
    if (v->at(i).when >= last)
      new_vec.push_back(v->at(i));
  
  _bcast_stats.insert(e, new_vec);
  
}

String
LinkStat::read_window(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  return String(f->_window) + "\n";
}



String
LinkStat::read_stats(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;

  String s;

  for (BigHashMap<EtherAddress, LinkStat::stat_t>::iterator i = f->_stats.begin(); i; i++) {
    char timebuf[80];
    snprintf(timebuf, 80, " %lu.%06lu", i.value().when.tv_sec, i.value().when.tv_usec);
    s += i.key().s() + String(timebuf) + " sig: " + String(i.value().sig) + ", qual: " + String(i.value().qual) + "\n";
  }
  return s;
}

String
LinkStat::read_bcast_stats(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;

  Vector<EtherAddress> e_vec;
  int num = 0;

  //  click_chatter("LS read_stats XXXX 1");

  String s;
  for (BigHashMap<EtherAddress, Vector<LinkStat::bcast_t> >::iterator i = f->_bcast_stats.begin(); i; i++) {
    e_vec.push_back(i.key());
    num++;
  }

  //  click_chatter("LS read_stats XXXX 2");

  for (int i = 0; i < num; i++) {
    // click_chatter("LS read_stats XXXX 3");
    struct timeval when;
    unsigned int window, num_rx, num_expected;    
    bool res = f->get_bcast_stats(e_vec[i], when, window, num_rx, num_expected);
    if (!res || window != f->_window) 
      return "Error: inconsistent data structures\n";

    char timebuf[80];
    snprintf(timebuf, 80, "%lu.%06lu", when.tv_sec, when.tv_usec);
    s += e_vec[i].s() + " last=" + String(timebuf) + " num_rx=" + String(num_rx) + " num_expected=" + String(num_expected) + "\n";
    //    click_chatter("LS read_stats XXXX 4");
  }
  return s;
}


int
LinkStat::write_window(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  LinkStat *e = (LinkStat *) el;
  int window = atoi(((String) arg).cc());
  if (window < 0)
    return errh->error("window must be >= 0");
  e->_window = window;

  /* clear all stats to avoid confusing data */
  e->_bcast_stats.clear();

  return 0;
}


void
LinkStat::add_handlers()
{
  add_read_handler("stats", read_stats, 0);
  add_read_handler("bcast_stats", read_bcast_stats, 0);
  add_read_handler("window", read_window, 0);
  add_write_handler("window", write_window, 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LinkStat)

#include <click/bighashmap.cc>
#include <click/vector.cc>
template class BigHashMap<EtherAddress, LinkStat::stat_t>;
template class Vector<LinkStat::bcast_t>;
template class BigHashMap<EtherAddress, Vector<LinkStat::bcast_t> >;
CLICK_ENDDECLS
