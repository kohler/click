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
#include <click/click_ether.h>
#include "linkstat.hh"
#include <click/glue.hh>
#include <sys/time.h>
#include "grid.hh"

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

  return p;
}

String
LinkStat::read_stats(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  String s;
  char timebuf[80];
  for (BigHashMap<EtherAddress, LinkStat::stat_t>::Iterator i = f->_stats.first(); i; i++) {
    snprintf(timebuf, 80, " %lu.%06lu", i.value().when.tv_sec, i.value().when.tv_usec);
    s += i.key().s() + timebuf + " sig: " + String(i.value().sig) + ", qual: " + String(i.value().qual) + "\n";
  }
  return s;
}

void
LinkStat::add_handlers()
{
  add_read_handler("stats", read_stats, 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LinkStat)

#include <click/bighashmap.cc>
template class BigHashMap<IPAddress, LinkStat::stat_t>;














