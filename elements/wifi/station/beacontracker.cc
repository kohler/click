/*
 * beacontracker.{cc,hh} -- tracks 802.11 beacon sources
 * John Bicket
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/availablerates.hh>
#include <elements/wifi/wirelessinfo.hh>
#include "beacontracker.hh"


CLICK_DECLS


#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y))


BeaconTracker::BeaconTracker()
  : _winfo(0)
{
}

BeaconTracker::~BeaconTracker()
{
}

int
BeaconTracker::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
		   "DEBUG", 0, cpBool, &_debug,
		   "WIRELESS_INFO", 0, cpElement, &_winfo,
		   "TRACK", 0, cpInteger, &_track,
		   cpEnd) < 0)
    return -1;


  reset();
  return 0;
}

Packet *
BeaconTracker::simple_action(Packet *p)
{
  uint8_t dir;
  uint8_t type;
  uint8_t subtype;


  if (p->length() < sizeof(struct click_wifi)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_wifi));

    p->kill();
    return 0;
	      
  }
  struct click_wifi *w = (struct click_wifi *) p->data();


  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%{element}: received non-management packet\n",
		  this);
    p->kill();
    return 0;
  }

  if (subtype != WIFI_FC0_SUBTYPE_BEACON && subtype != WIFI_FC0_SUBTYPE_PROBE_RESP) {
    click_chatter("%{element}: received subtype %d packet\n",
		  this,
		  subtype);
    p->kill();
    return 0;
  }


  EtherAddress bssid = EtherAddress(w->i_addr3);
  
  if (_winfo->_bssid != bssid) {
    p->kill();
    return 0;
  }

  uint8_t *ptr;
  
  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  //uint8_t *ts = ptr;
  ptr += 8;

  uint16_t beacon_int = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;


  struct beacon_t b;
  uint16_t seq = le16_to_cpu(*(uint16_t *) w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;

  
  b.rx = p->timestamp_anno();
  b.seq = seq;
  
  _beacons.push_back(b);
  _beacon_int = beacon_int;

  trim();
  return p;
}

void
BeaconTracker::trim() 
{
  Timestamp earliest = Timestamp::now() - Timestamp::make_msec(_track * _beacon_int);
  while (_beacons.size() && _beacons[0].rx < earliest) {
    _beacons.pop_front();
  }  

}

void 
BeaconTracker::reset() 
{
  click_gettimeofday(&_start);
  _beacons.clear();
}

enum {H_DEBUG, H_SCAN, H_RESET, H_STATS, 
      H_TRACK, H_BEACON_INTERVAL};

static String 
read_param(Element *e, void *thunk)
{
  BeaconTracker *td = (BeaconTracker *)e;
    switch ((uintptr_t) thunk) {
    case H_DEBUG:
      return String(td->_debug) + "\n";
    case H_BEACON_INTERVAL:
      return String(td->_beacon_int) + "\n";
    case H_TRACK:
      return String(td->_track) + "\n";
    case H_STATS: {
      struct timeval now;
      struct timeval diff;
      td->trim();
      click_gettimeofday(&now);
      timersub(&now, &td->_start, &diff);
      int expected = min(1000*diff.tv_sec + diff.tv_usec/1000, td->_track);
      int count = td->_beacons.size();
      int p = expected ? count*100/expected : 0;
      return String(p) + "\n";
    }
      
    default:
      return String();
    }
}
static int 
write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BeaconTracker *f = (BeaconTracker *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_RESET: {    //reset
    f->reset();
  }
  }
  return 0;
}
 
void
BeaconTracker::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_read_handler("scan", read_param, (void *) H_SCAN);
  add_read_handler("stats", read_param, (void *) H_STATS);
  add_read_handler("track", read_param, (void *) H_TRACK);
  add_read_handler("beacon_interval", read_param, (void *) H_BEACON_INTERVAL);

  add_write_handler("debug", write_param, (void *) H_DEBUG);
  add_write_handler("reset", write_param, (void *) H_RESET, Handler::BUTTON);
}


#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class DEQueue<LinkStat::beacon_t>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(BeaconTracker)
