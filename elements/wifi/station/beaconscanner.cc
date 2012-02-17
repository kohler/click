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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/availablerates.hh>
#include <elements/wifi/wirelessinfo.hh>
#include "beaconscanner.hh"
CLICK_DECLS

BeaconScanner::BeaconScanner()
  : _rtable(0),
    _winfo(0)
{
}

BeaconScanner::~BeaconScanner()
{
}

int
BeaconScanner::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (Args(conf, this, errh)
      .read("DEBUG", _debug)
      .read("WIRELESS_INFO", ElementCastArg("WirelessInfo"), _winfo)
      .read_m("RT", ElementCastArg("AvailableRates"), _rtable)
      .complete() < 0)
    return -1;

  return 0;
}

Packet *
BeaconScanner::simple_action(Packet *p)
{


  uint8_t dir;
  uint8_t type;
  uint8_t subtype;


  if (_winfo && _winfo->_channel < 0) {
    return p;
  }

  if (p->length() < sizeof(struct click_wifi)) {
    return p;
  }
  struct click_wifi *w = (struct click_wifi *) p->data();


  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    return p;
  }

  if (subtype != WIFI_FC0_SUBTYPE_BEACON && subtype != WIFI_FC0_SUBTYPE_PROBE_RESP) {
    return p;
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
	click_chatter("%p{element}: ignored element id %u %u \n",
		      this,
		      *ptr,
		      ptr[1]);
      }
    }
    ptr += ptr[1] + 2;

  }


  if (_winfo && _winfo->_channel > 0 && ds_l && ds_l[2] != _winfo->_channel) {
    return p;
  }
  String ssid = "";
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, WIFI_MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  }

  EtherAddress bssid = EtherAddress(w->i_addr3);

  wap *ap = _waps.findp(bssid);
  if (!ap) {
    _waps.insert(bssid, wap());
    ap = _waps.findp(bssid);
    ap->_ssid = "";
  }

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  ap->_eth = bssid;
  if (ssid != "") {
    /* don't overwrite blank ssids */
    ap->_ssid = ssid;
  }
  ap->_channel = (ds_l) ? ds_l[2] : -1;
  ap->_rssi = ceh->rssi;

  ap->_capability = capability;
  ap->_beacon_int = beacon_int;
  ap->_basic_rates.clear();
  ap->_rates.clear();
  Vector<int> all_rates;
  ap->_last_rx.assign_now();
  if (rates_l) {
    for (int x = 0; x < WIFI_MIN((int)rates_l[1], WIFI_RATE_SIZE); x++) {
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
    for (int x = 0; x < WIFI_MIN((int)xrates_l[1], WIFI_RATE_SIZE); x++) {
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
  Timestamp now = Timestamp::now();
  for (APIter iter = _waps.begin(); iter.live(); iter++) {
    wap ap = iter.value();
    sa << ap._eth << " ";
    sa << "channel " << ap._channel << " ";
    sa << "rssi " << ap._rssi << " ";
    sa << "ssid ";

    if(ap._ssid == "") {
      sa << "(none) ";
    } else {
      sa << ap._ssid << " ";
    }
    sa << "beacon_interval " << ap._beacon_int << " ";
    sa << "last_rx " << now - ap._last_rx << " ";

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

enum {H_DEBUG, H_SCAN, H_RESET};

static String
BeaconScanner_read_param(Element *e, void *thunk)
{
  BeaconScanner *td = (BeaconScanner *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
      case H_SCAN:
	return td->scan_string();
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
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!BoolArg().parse(s, debug))
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
BeaconScanner::add_handlers()
{
  add_read_handler("debug", BeaconScanner_read_param, H_DEBUG);
  add_read_handler("scan", BeaconScanner_read_param, H_SCAN);

  add_write_handler("debug", BeaconScanner_write_param, H_DEBUG);
  add_write_handler("reset", BeaconScanner_write_param, H_RESET, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BeaconScanner)
