/*
 * beaconscanner.{cc,hh} -- tracks 802.11 beacon sources
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
#include "beaconscanner.hh"


CLICK_DECLS


#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y))


BeaconScanner::BeaconScanner()
  : Element(1, 1),
    _rtable(0)
{
  MOD_INC_USE_COUNT;
}

BeaconScanner::~BeaconScanner()
{
  MOD_DEC_USE_COUNT;
}

int
BeaconScanner::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _channel = 0;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  "CHANNEL", cpInteger, "channel", &_channel,
		  "RT", cpElement, "availablerates", &_rtable,
		  cpEnd) < 0)
    return -1;

  if (!_rtable || _rtable->cast("AvailableRates") == 0) 
    return errh->error("AvailableRates element is not provided or not a AvailableRates");

  return 0;
}

Packet *
BeaconScanner::simple_action(Packet *p)
{


  uint8_t dir;
  uint8_t type;
  uint8_t subtype;


  if (_channel < 0) {
    return p;
  }

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

  uint8_t *ptr;
  
  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  //uint8_t *ts = ptr;
  ptr += 8;

  uint16_t beacon_int = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;


  uint8_t *end  = (uint8_t *) p->data() + p->length();

  uint8_t *ssid_l = NULL;
  uint8_t *rates_l = NULL;
  uint8_t *xrates_l = NULL;
  uint8_t *ds_l = NULL;
  while (ptr < end) {
    switch (*ptr) {
    case WIFI_ELEMID_SSID:
      ssid_l = ptr;
      break;
    case WIFI_ELEMID_RATES:
      rates_l = ptr;
      break;
    case WIFI_ELEMID_XRATES:
      xrates_l = ptr;
      break;
    case WIFI_ELEMID_FHPARMS:
      break;
    case WIFI_ELEMID_DSPARMS:
      ds_l = ptr;
      break;
    case WIFI_ELEMID_IBSSPARMS:
      break;
    case WIFI_ELEMID_TIM:
      break;
    case WIFI_ELEMID_ERP:
      break;
    case WIFI_ELEMID_VENDOR:
      break;
    case 133: /* ??? */
      break;
    case 150: /* ??? */
      break;
    default:
      if (_debug) {
	click_chatter("%{element}: ignored element id %u %u \n",
		      this,
		      *ptr,
		      ptr[1]);
      }
    }
    ptr += ptr[1] + 2;

  }


  if (_channel > 0 && ds_l && ds_l[2] != _channel) {
    return p;
  }
  String ssid = "";
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, min((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  }

  EtherAddress bssid = EtherAddress(w->i_addr3);

  wap *ap = _waps.findp(bssid);
  if (!ap) {
    _waps.insert(bssid, wap());
    ap = _waps.findp(bssid);
    ap->_ssid = "";
  }

  
  ap->_eth = bssid;
  if (ssid != "") {
    /* don't overwrite blank ssids */
    ap->_ssid = ssid;
  }
  ap->_channel = (ds_l) ? ds_l[2] : -1;
  ap->_rssi = WIFI_SIGNAL_ANNO(p);

  ap->_capability = capability;
  ap->_beacon_int = beacon_int;
  ap->_basic_rates.clear();
  ap->_rates.clear();
  Vector<int> all_rates;
  click_gettimeofday(&ap->_last_rx);
  if (rates_l) {
    for (int x = 0; x < min((int)rates_l[1], WIFI_RATE_SIZE); x++) {
      uint8_t rate = rates_l[x + 2];
      
      if (rate & WIFI_RATE_BASIC) {
	ap->_basic_rates.push_back((int)(rate & WIFI_RATE_VAL));
      } else {
	ap->_rates.push_back((int)(rate & WIFI_RATE_VAL));
      }
      all_rates.push_back((int)(rate & WIFI_RATE_VAL));
    }
  }

  
  if (xrates_l) {
    for (int x = 0; x < min((int)xrates_l[1], WIFI_RATE_SIZE); x++) {
      uint8_t rate = xrates_l[x + 2];
      
      if (rate & WIFI_RATE_BASIC) {
	ap->_basic_rates.push_back((int)(rate & WIFI_RATE_VAL));
      } else {
	ap->_rates.push_back((int)(rate & WIFI_RATE_VAL));
      }
      all_rates.push_back((int)(rate & WIFI_RATE_VAL));
    }
  }

  if (_rtable) {
    _rtable->insert(bssid, all_rates);
  }
  
  return p;
}


String
BeaconScanner::scan_string()
{
  StringAccum sa;
  struct timeval now;
  click_gettimeofday(&now);
  for (APIter iter = _waps.begin(); iter; iter++) {
    wap ap = iter.value();
    sa << ap._eth << " ";
    sa << "channel " << ap._channel << " ";
    sa << "ssid ";
    if(ap._ssid == "") {
      sa << "(none) ";
    } else {
      sa << ap._ssid << " ";
    }

    sa << "last_rx " << now - ap._last_rx << " ";
    sa << "rssi " << ap._rssi << " ";
    sa << "beacon_interval " << ap._beacon_int << " ";
    sa << "[ ";
    if (ap._capability & WIFI_CAPINFO_ESS) {
      sa << "ESS ";
    }
    if (ap._capability & WIFI_CAPINFO_IBSS) {
      sa << "IBSS ";
    }
    if (ap._capability & WIFI_CAPINFO_CF_POLLABLE) {
      sa << "CF_POLLABLE ";
    }
    if (ap._capability & WIFI_CAPINFO_CF_POLLREQ) {
      sa << "CF_POLLREQ ";
    }
    if (ap._capability & WIFI_CAPINFO_PRIVACY) {
      sa << "PRIVACY ";
    }
    sa << "] ";

    sa << "( { ";
    for (int x = 0; x < ap._basic_rates.size(); x++) {
      sa << ap._basic_rates[x] << " ";
    }
    sa << "} ";
    for (int x = 0; x < ap._rates.size(); x++) {
      sa << ap._rates[x] << " ";
    }

    sa << ")\n";
  }
  return sa.take_string();
}


void 
BeaconScanner::reset() 
{
  _waps.clear();
}

enum {H_DEBUG, H_SCAN, H_RESET, H_CHANNEL};

static String 
BeaconScanner_read_param(Element *e, void *thunk)
{
  BeaconScanner *td = (BeaconScanner *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
      case H_SCAN:
	return td->scan_string();
      case H_CHANNEL:
	return String(td->_channel) + "\n";
    default:
      return String();
    }
}
static int 
BeaconScanner_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BeaconScanner *f = (BeaconScanner *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
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
  case H_CHANNEL: {    //channel
    int channel;
    if (!cp_integer(s, &channel)) 
      return errh->error("channel parameter must be integer");
    f->_channel = channel;
    break;
  }

  }
  return 0;
}
 
void
BeaconScanner::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", BeaconScanner_read_param, (void *) H_DEBUG);
  add_read_handler("scan", BeaconScanner_read_param, (void *) H_SCAN);
  add_read_handler("channel", BeaconScanner_read_param, (void *) H_CHANNEL);

  add_write_handler("debug", BeaconScanner_write_param, (void *) H_DEBUG);
  add_write_handler("reset", BeaconScanner_write_param, (void *) H_RESET);
  add_write_handler("channel", BeaconScanner_write_param, (void *) H_CHANNEL);
}


#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(BeaconScanner)
