/*
 * dupefilter.{cc,hh} -- print sr packets, for debugging.
 * John Bicket
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
#include "dupefilter.hh"

CLICK_DECLS

DupeFilter::DupeFilter()
  : Element(1, 1),
    _window(10)
{
  MOD_INC_USE_COUNT;
}

DupeFilter::~DupeFilter()
{
  MOD_DEC_USE_COUNT;
}

DupeFilter *
DupeFilter::clone() const
{
  return new DupeFilter;
}

int
DupeFilter::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "WINDOW", cpInteger, "window length", &_window,
		    cpEnd);
  return ret;
}

Packet *
DupeFilter::simple_action(Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  Path p = pk->get_path();
  PathInfo *nfo = _paths.findp(p);
  struct timeval now;

  click_gettimeofday(&now);

  if (!nfo) {
    _paths.insert(p, PathInfo(p));
    nfo = _paths.findp(p);
    nfo->clear();
  }

  int seq = pk->data_seq();
  if (0 == seq || (now.tv_sec - nfo->_last.tv_sec > 30)) {
    /* reset */
    nfo->clear();
  }
  
  for (int x = 0; x < nfo->_sequences.size(); x++) {
    if(seq == nfo->_sequences[x]) {
      /* duplicate dectected */
      nfo->_dupes++;
      p_in->kill();
      return 0;
    }
  }

  nfo->_packets++;
  nfo->_last = now;
  nfo->_sequences.push_back(seq);

  while(nfo->_sequences.size() > _window) {
    nfo->_sequences.pop_front();
  }
  
  return p_in;
}


String
DupeFilter::read_stats(Element *xf, void *) 
{
  DupeFilter *e = (DupeFilter *) xf;
  StringAccum sa;
  struct timeval now;

  click_gettimeofday(&now);

  for(PathTable::const_iterator i = e->_paths.begin(); i; i++) {
    PathInfo nfo = i.value();
    sa << "age " << now - nfo._last;
    sa << " packets " << nfo._packets;
    sa << " dupes " << nfo._dupes;
    sa << " seq_size " << nfo._sequences.size();
    sa << " [ " << path_to_string(nfo._p) << " ]\n";
  }
  return sa.take_string();
}

void
DupeFilter::add_handlers() 
{
  add_default_handlers(true);
  add_read_handler("stats", read_stats, 0);
}

EXPORT_ELEMENT(DupeFilter)

#include <click/hashmap.cc>
#include <click/dequeue.cc>
template class DEQueue<int>;
CLICK_ENDDECLS

