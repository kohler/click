/*
 * wifidupefilter.{cc,hh} -- Filters out duplicate packets based on 
 * 802.11 sequence numbers.
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/confparse.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include "wifidupefilter.hh"

CLICK_DECLS

WifiDupeFilter::WifiDupeFilter()
  : Element(1, 1),
    _window(10),
    _debug(false),
    _dupes(0),
    _packets(0)
{
  MOD_INC_USE_COUNT;
}

WifiDupeFilter::~WifiDupeFilter()
{
  MOD_DEC_USE_COUNT;
}

int
WifiDupeFilter::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "WINDOW", cpInteger, "window length", &_window,
		    "DEBUG", cpBool, "debug level", &_debug,
		    cpEnd);
  return ret;
}

Packet *
WifiDupeFilter::simple_action(Packet *p_in)
{
  struct timeval now;
  click_wifi *w = (click_wifi *) p_in->data();
  EtherAddress src = EtherAddress(w->i_addr2);
  uint16_t seq = le16_to_cpu(*(uint16_t *) w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
  //uint8_t fragno = le16_to_cpu(*(u_int16_t *)w->i_seq) & WIFI_SEQ_FRAG_MASK;
  DstInfo *nfo = _table.findp(src);

  click_gettimeofday(&now);
  _packets++;
  if (!nfo) {
    _table.insert(src, DstInfo(src));
    nfo = _table.findp(src);
    nfo->clear();
  }

  if (0 == seq || (now.tv_sec - nfo->_last.tv_sec > 30)) {
    /* reset */
    if (_debug) {
      click_chatter("%{element}: reset seq %d src %s\n",
		    this,
		    seq,
		    src.s().cc());
    }
    nfo->clear();
  }
  
  for (int x = 0; x < nfo->_sequences.size(); x++) {
    if(seq == nfo->_sequences[x]) {
      /* duplicate dectected */
      if (_debug) {
	click_chatter("%{element}: dup seq %d src %s\n",
		      this,
		      seq,
		      src.s().cc());
      }
      nfo->_dupes++;
      _dupes++;
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
WifiDupeFilter::static_read_stats(Element *xf, void *) 
{
  WifiDupeFilter *e = (WifiDupeFilter *) xf;
  StringAccum sa;
  struct timeval now;

  click_gettimeofday(&now);

  for(DstTable::const_iterator i = e->_table.begin(); i; i++) {
    DstInfo nfo = i.value();
    sa << nfo._eth;
    sa << " age " << now - nfo._last;
    sa << " packets " << nfo._packets;
    sa << " dupes " << nfo._dupes;
    sa << " seq_size " << nfo._sequences.size();
    sa << " [";
    for (int x = 0; x < nfo._sequences.size(); x++) {
      sa << " " << nfo._sequences[x];
    }
    sa << "]\n";

  }
  return sa.take_string();
}

enum {H_DEBUG, H_WINDOW, H_DUPES, H_PACKETS, H_RESET};

static String 
WifiDupeFilter_read_param(Element *e, void *thunk)
{
  WifiDupeFilter *td = (WifiDupeFilter *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
      case H_WINDOW:
	return String(td->_window) + "\n";
      case H_DUPES:
	return String(td->_dupes) + "\n";
      case H_PACKETS:
	return String(td->_packets) + "\n";
    default:
      return String();
    }
}
static int 
WifiDupeFilter_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WifiDupeFilter *f = (WifiDupeFilter *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_WINDOW: {    
    int window;
    if (!cp_integer(s, &window)) 
      return errh->error("window parameter must be integer");
    f->_window = window;
    break;
  }
  case H_RESET: {
    f->_table.clear();
    f->_packets = 0;
    f->_dupes = 0;
  }
  }
  return 0;
}
void
WifiDupeFilter::add_handlers() 
{
  add_default_handlers(true);
  
  add_read_handler("debug", WifiDupeFilter_read_param, (void *) H_DEBUG);
  add_read_handler("window", WifiDupeFilter_read_param, (void *) H_WINDOW);
  add_read_handler("dupes", WifiDupeFilter_read_param, (void *) H_DUPES);
  add_read_handler("packets", WifiDupeFilter_read_param, (void *) H_PACKETS);

  add_write_handler("debug", WifiDupeFilter_write_param, (void *) H_DEBUG);
  add_write_handler("window", WifiDupeFilter_write_param, (void *) H_WINDOW);
  add_write_handler("reset", WifiDupeFilter_write_param, (void *) H_RESET);

  add_read_handler("stats", static_read_stats, 0);
}

EXPORT_ELEMENT(WifiDupeFilter)

#include <click/hashmap.cc>
#include <click/dequeue.cc>
template class DEQueue<int>;
CLICK_ENDDECLS

