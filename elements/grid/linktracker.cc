/*
 * linktracker.{cc,hh} -- track link quality/strength stats
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
#include "linktracker.hh"
#include <click/glue.hh>
#include <sys/time.h>
#include "grid.hh"
#include <math.h>
#include "timeutils.hh"
CLICK_DECLS

LinkTracker::LinkTracker()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

LinkTracker::~LinkTracker()
{
  MOD_DEC_USE_COUNT;
}

LinkTracker *
LinkTracker::clone() const
{
  return new LinkTracker();
}


int
LinkTracker::configure(Vector<String> &conf, ErrorHandler *errh)
{
  unsigned int tau_int = 0;
  int res = cp_va_parse(conf, this, errh,
			cpUnsigned, "time constant (tau) (milliseconds)", &tau_int,
			0);
  if (res < 0)
    return res;
  
  _tau = tau_int;
  _tau *= 0.001;
  
  return res;
}



int
LinkTracker::initialize(ErrorHandler *)
{
  return 0;
}

void
LinkTracker::remove_all_stats(IPAddress dst)
{
  _stats.remove(dst);
  _bcast_stats.remove(dst);
}
  

void
LinkTracker::add_stat(IPAddress dst, int sig, int qual, struct timeval when)
{
  if (sig == 0 && qual == 0) {
    click_chatter("LinkTracker: ignoring probably bad link info from %s\n",
		  dst.s().cc());
    return;
  }

  struct timeval now;
  gettimeofday(&now, 0);

  stat_t *s = _stats.findp(dst);
  if (s == 0) {
    /* init new entry */
    stat_t s2;
    s2.last_data = when;
    s2.last_update = now;
    
    s2.qual_top = qual;
    s2.qual_bot = 1.0;
    
    s2.sig_top = sig;
    s2.sig_bot = 1.0;
   
    _stats.insert(dst, s2);
  }
  else {
    /* assumes ``when'' is later in time than last data point */
    timeval tv = when - s->last_data;
    if (tv.tv_sec == 0 && tv.tv_usec == 0) {
      /* this isn't new data, just a repeat of an old statistic.  it
         may be true that packets can arrive at a node and stats can
         be generated faster than once per usec, (or whatever the
         gettimeofday granularity is), but we won't worry about that */
      return; 
    }
    double delta = tv.tv_sec;
    delta += tv.tv_usec / 1.0e6;
    
    double old_weight = exp(-delta / _tau);

    s->qual_top *= old_weight;
    s->qual_top += qual;

    s->qual_bot *= old_weight;
    s->qual_bot += 1.0;

    s->sig_top *= old_weight;
    s->sig_top += sig;

    s->sig_bot *= old_weight;
    s->sig_bot += 1.0;

    s->last_data = when;
    s->last_update = now;

    if (s->sig_top > 1e100) {
      click_chatter("LinkTracker: warning, signal strength accumulators are getting really big!!!  Renormalizing.\n");
      s->sig_top *= 1e-80;
      s->sig_bot *= 1e-80;
    }
    if (s->qual_top > 1e100) {
      click_chatter("LinkTracker: warning, signal quality accumulators are getting really big!!!  Renormalizing.\n");
      s->qual_top *= 1e-80;
      s->qual_bot *= 1e-80;
    }
  }
}


bool
LinkTracker::get_stat(IPAddress dst, int &sig, int &qual, struct timeval &last_update)
{
  stat_t *s = _stats.findp(dst);
  if (s == 0)
    return false;

  sig = (int) (s->sig_top / s->sig_bot);
  qual = (int) (s->qual_top / s->qual_bot);
  last_update = s->last_update;

  return true;
}

void 
LinkTracker::add_bcast_stat(IPAddress dst, unsigned int num_rx, unsigned int num_expected, struct timeval last_bcast)
{
  /* can only believe num_expected if the receiver heard at least 2 packets */
  if (num_rx < 2)
    return;

  if (num_rx > num_expected) 
    click_chatter("LinkTracker::add_bcast_stat WARNING num_rx (%d) > num_expected (%d) for %s",
		  num_rx, num_expected, dst.s().cc());

  double num_rx_ = num_rx;
  double num_expected_ = num_expected;

  /* 
   * calculate loss rate, being pessimistic.  that is, choose the loss
   * rate r such that: 
   *
   * (r * num_expected) + 0.5 = num_rx
   *
   * this makes r the lowest rate such that (r * num_expected) rounds
   * up to the number of packets actually received.  
   */
  double r = (num_rx_ - 0.5) / num_expected_;
    
  struct timeval now;
  gettimeofday(&now, 0);
  
  bcast_t *s = _bcast_stats.findp(dst);
  if (s == 0) {
    /* init entry */
    bcast_t s2;
    s2.last_bcast = last_bcast;
    s2.last_update = now;

    s2.r_top = r;
    s2.r_bot = 1.0;

    _bcast_stats.insert(dst, s2);
  }
  else {
    timeval tv = last_bcast - s->last_bcast;
    if (tv.tv_sec == 0 && tv.tv_usec == 0) 
      return; // repeat of old data
    
    double delta = tv.tv_sec;
    delta += tv.tv_usec / 1.0e6;
    
    double old_weight = exp(-delta / _tau);
    
    s->r_top *= old_weight;
    s->r_top += r;

    s->r_bot *= old_weight;
    s->r_bot += 1.0;

    if (s->r_top > 1e100) {
      click_chatter("LinkTracker: warning, broadcast delivery rate accumulators are getting really big!!!  Renormalizing.\n");
      s->r_top *= 1e-80;
      s->r_bot *= 1e-80;
    }
  }
}
 

bool
LinkTracker::get_bcast_stat(IPAddress dst, double &delivery_rate, struct timeval &last_update)
{
  bcast_t *s = _bcast_stats.findp(dst);
  if (s == 0)
    return false;

  delivery_rate = s->r_top / s->r_bot;
  last_update = s->last_update;

  return true;
}


Packet *
LinkTracker::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (eh + 1);

  IPAddress ip(gh->tx_ip);

  grid_nbr_encap *nb = 0;

  switch (gh->type) {
  case grid_hdr::GRID_NBR_ENCAP: 
  case grid_hdr::GRID_LOC_REPLY:
  case grid_hdr::GRID_ROUTE_PROBE: 
  case grid_hdr::GRID_ROUTE_REPLY: 
    nb = (grid_nbr_encap *) (gh + 1);
    struct timeval tv;
    tv.tv_sec = ntohl(nb->measurement_time.tv_sec);
    tv.tv_usec = ntohl(nb->measurement_time.tv_usec);
    add_stat(ip, ntohl(nb->link_sig), ntohl(nb->link_qual), tv);
    break;
  default:
    ;
  }

  return p;
}

String
LinkTracker::read_tau(Element *xf, void *)
{
  LinkTracker *f = (LinkTracker *) xf;
  int tau = (int) (f->_tau * 1000);
  return String(tau) + "\n";
}


String
LinkTracker::read_stats(Element *xf, void *)
{
  LinkTracker *f = (LinkTracker *) xf;

  char timebuf[80];
  String s;
  for (BigHashMap<IPAddress, LinkTracker::stat_t>::iterator i = f->_stats.begin(); i; i++) {
    snprintf(timebuf, 80, " %lu.%06lu", i.value().last_update.tv_sec, i.value().last_update.tv_usec);
    s += i.key().s() 
      + String(timebuf)
      + " sig: " + String(i.value().sig_top / i.value().sig_bot) 
      + ", qual: " + String(i.value().qual_top / i.value().qual_bot) + "\n";
  }
  return s;
}

String
LinkTracker::read_bcast_stats(Element *xf, void *)
{
  LinkTracker *f = (LinkTracker *) xf;

  char timebuf[80];
  String s;
  for (BigHashMap<IPAddress, LinkTracker::bcast_t>::iterator i = f->_bcast_stats.begin(); i; i++) {
    snprintf(timebuf, 80, " %lu.%06lu", i.value().last_update.tv_sec, i.value().last_update.tv_usec);
    s += i.key().s() 
      + String(timebuf)
      + " " + String(i.value().r_top / i.value().r_bot) + "\n";
  }
  return s;
}


int
LinkTracker::write_tau(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  LinkTracker *e = (LinkTracker *) el;
  double tau = atof(((String) arg).cc());
  if (tau < 0)
    return errh->error("tau must be >= 0");
  e->_tau = tau * 0.001;

  /* clear all stats to avoid confusing data averaged underone time
     constant to data averaged under a different time constant. */
  e->_stats.clear();
  e->_bcast_stats.clear();

  return 0;
}


void
LinkTracker::add_handlers()
{
  add_read_handler("stats", read_stats, 0);
  add_read_handler("bcast_stats", read_bcast_stats, 0);
  add_read_handler("tau", read_tau, 0);
  add_write_handler("tau", write_tau, 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LinkTracker)

#include <click/bighashmap.cc>
template class BigHashMap<IPAddress, LinkTracker::stat_t>;
template class BigHashMap<IPAddress, LinkTracker::bcast_t>;
CLICK_ENDDECLS
