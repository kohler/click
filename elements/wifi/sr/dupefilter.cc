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
#include <click/confparse.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
#include "dupefilter.hh"

CLICK_DECLS

DupeFilter::DupeFilter()
  : _window(10),
    _debug(0)
{
}

DupeFilter::~DupeFilter()
{
}

int
DupeFilter::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "WINDOW", cpInteger, "window length", &_window,
		    "DEBUG", cpInteger, "debug level", &_debug,
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
    if (_debug > 2) {
      click_chatter("%{element}: reset seq %d path %s\n",
		    this,
		    seq,
		    path_to_string(p).c_str());
    }
    nfo->clear();
  }
  
  for (int x = 0; x < nfo->_sequences.size(); x++) {
    if(seq == nfo->_sequences[x]) {
      /* duplicate dectected */
      if (_debug > 2) {
	click_chatter("%{element}: dup seq %d path %s\n",
		      this,
		      seq,
		      path_to_string(p).c_str());
      }
      nfo->_dupes++;
      p_in->kill();
      return 0;
    }
  }

  nfo->_packets++;
  nfo->_last = now;
  nfo->_sequences.push_back(seq);
  /* clear space for new seq */
  while( nfo->_sequences.size() > _window) {
    nfo->_sequences.pop_front();
  }


  return p_in;
}


String
DupeFilter::static_read_stats(Element *xf, void *) 
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
    sa << "[";
    for (int x = 0; x < nfo._sequences.size(); x++) {
      sa << " " << nfo._sequences[x];
    }
    sa << "]\n";

  }
  return sa.take_string();
}

String
DupeFilter::static_read_debug(Element *f, void *)
{
  StringAccum sa;
  DupeFilter *d = (DupeFilter *) f;
  sa << d->_debug << "\n";
  return sa.take_string();
}
int
DupeFilter::static_write_debug(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  DupeFilter *n = (DupeFilter *) e;
  int i;

  if (!cp_integer(arg, &i))
    return errh->error("`debug' must be a integer");

  n->_debug = i;
  return 0;
}
void
DupeFilter::add_handlers() 
{
  add_read_handler("stats", static_read_stats, 0);
  add_read_handler("debug", static_read_debug, 0);
  add_write_handler("debug", static_write_debug, 0);
}

EXPORT_ELEMENT(DupeFilter)

#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class DEQueue<int>;
#endif
CLICK_ENDDECLS

