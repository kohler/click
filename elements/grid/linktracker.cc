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
#include <click/click_ether.h>
#include <click/error.hh>
#include "linktracker.hh"
#include <click/glue.hh>
#include <sys/time.h>
#include "grid.hh"
#include <math.h>

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

inline timeval
operator- (const timeval &a, const timeval &b)
{
  timeval ts;
  ts.tv_sec = a.tv_sec - b.tv_sec;
  if (a.tv_usec > b.tv_usec)
    ts.tv_usec = a.tv_usec - b.tv_usec;
  else {
    ts.tv_usec = a.tv_usec + 1000000 - b.tv_usec;
    --ts.tv_sec;
  }
  return ts;
}


void
LinkTracker::remove_stat(IPAddress dst)
{
  _stats.remove(dst);
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
    /* assumes ``when'' is late in time than last data point */
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
  for (BigHashMap<IPAddress, LinkTracker::stat_t>::Iterator i = f->_stats.first(); i; i++) {
    snprintf(timebuf, 80, " %lu.%06lu", i.value().last_update.tv_sec, i.value().last_update.tv_usec);
    s += i.key().s() 
      + timebuf
      + " sig: " + String(i.value().sig_top / i.value().sig_bot) 
      + ", qual: " + String(i.value().qual_top / i.value().qual_bot) + "\n";
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

  return 0;
}


void
LinkTracker::add_handlers()
{
  add_read_handler("stats", read_stats, 0);
  add_read_handler("tau", read_tau, 0);
  add_write_handler("tau", write_tau, 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LinkTracker)

#include <click/bighashmap.cc>
template class BigHashMap<IPAddress, LinkTracker::stat_t>;
