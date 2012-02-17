/*
 * beaconsource.{cc,hh} -- sends 802.11 beacon packets
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
#include <click/timer.hh>
#include <click/error.hh>
#include "beaconsource.hh"
#include <elements/wifi/availablerates.hh>
#include <elements/wifi/wirelessinfo.hh>
CLICK_DECLS

BeaconSource::BeaconSource()
  : _timer(this),
    _rtable(0)
{
  _bcast = EtherAddress();
  memset(_bcast.data(), 0xff, 6);
}

BeaconSource::~BeaconSource()
{
}

int
BeaconSource::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (Args(conf, this, errh)
      .read("DEBUG", _debug)
      .read_m("WIRELESS_INFO", ElementCastArg("WirelessInfo"), _winfo)
      .read_m("RT", ElementCastArg("AvailableRates"), _rtable)
      .complete() < 0)
    return -1;

  if (_winfo->_interval <= 0) {
    return errh->error("INTERVAL must be >0\n");
  }

  return 0;
}

int
BeaconSource::initialize (ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_msec(_winfo->_interval);
  return 0;
}

void
BeaconSource::run_timer(Timer *)
{
  send_beacon(_bcast, false);
  _timer.schedule_after_msec(_winfo->_interval);
}

void
BeaconSource::send_beacon(EtherAddress dst, bool probe)
{
  EtherAddress bssid = _winfo ? _winfo->_bssid : EtherAddress();
  String my_ssid = _winfo ? _winfo->_ssid : "";
  Vector<int> rates = _rtable->lookup(bssid);

  /* order elements by standard
   * needed by sloppy 802.11b driver implementations
   * to be able to connect to 802.11g APs */
  int max_len = sizeof (struct click_wifi) +
    8 +                  /* timestamp */
    2 +                  /* beacon interval */
    2 +                  /* cap_info */
    2 + my_ssid.length() + /* ssid */
    2 + WIFI_RATES_MAXSIZE +  /* rates */
    2 + 1 +              /* ds parms */
    2 + 4 +              /* tim */
    /* 802.11g Information fields */
    2 + WIFI_RATES_MAXSIZE +  /* xrates */
    0;


  WritablePacket *p = Packet::make(max_len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT;
  if (probe) {
    w->i_fc[0] |= WIFI_FC0_SUBTYPE_PROBE_RESP;
  } else {
    w->i_fc[0] |=  WIFI_FC0_SUBTYPE_BEACON;
  }

  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memcpy(w->i_addr1, dst.data(), 6);
  memcpy(w->i_addr2, bssid.data(), 6);
  memcpy(w->i_addr3, bssid.data(), 6);

  w->i_dur = 0;
  w->i_seq = 0;

  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
  int actual_length = sizeof (struct click_wifi);


  /* timestamp is set in the hal. ??? */
  memset(ptr, 0, 8);
  ptr += 8;
  actual_length += 8;

  uint16_t beacon_int = (uint16_t) _winfo->_interval;
  *(uint16_t *)ptr = cpu_to_le16(beacon_int);
  ptr += 2;
  actual_length += 2;

  uint16_t cap_info = 0;
  cap_info |= WIFI_CAPINFO_ESS;
  *(uint16_t *)ptr = cpu_to_le16(cap_info);
  ptr += 2;
  actual_length += 2;

  /* ssid */
  ptr[0] = WIFI_ELEMID_SSID;
  ptr[1] = my_ssid.length();
  memcpy(ptr + 2, my_ssid.data(), my_ssid.length());
  ptr += 2 + my_ssid.length();
  actual_length += 2 + my_ssid.length();

  /* rates */
  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = WIFI_MIN(WIFI_RATE_SIZE, rates.size());
  for (int x = 0; x < WIFI_MIN(WIFI_RATE_SIZE, rates.size()); x++) {
    ptr[2 + x] = (uint8_t) rates[x];

    if (rates[x] == 2) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }

  }
  ptr += 2 + WIFI_MIN(WIFI_RATE_SIZE, rates.size());
  actual_length += 2 + WIFI_MIN(WIFI_RATE_SIZE, rates.size());


  /* channel */
  ptr[0] = WIFI_ELEMID_DSPARMS;
  ptr[1] = 1;
  ptr[2] = (uint8_t) _winfo->_channel;
  ptr += 2 + 1;
  actual_length += 2 + 1;

  /* tim */

  ptr[0] = WIFI_ELEMID_TIM;
  ptr[1] = 4;

  ptr[2] = 0; //count
  ptr[3] = 1; //period
  ptr[4] = 0; //bitmap control
  ptr[5] = 0; //paritial virtual bitmap
  ptr += 2 + 4;
  actual_length += 2 + 4;

  /* 802.11g fields */
  /* extended supported rates */
  int num_xrates = rates.size() - WIFI_RATE_SIZE;
  if (num_xrates > 0) {
    /* rates */
    ptr[0] = WIFI_ELEMID_XRATES;
    ptr[1] = num_xrates;
    for (int x = 0; x < num_xrates; x++) {
      ptr[2 + x] = (uint8_t) rates[x + WIFI_RATE_SIZE];

      if (rates[x + WIFI_RATE_SIZE] == 2) {
	ptr [2 + x] |= WIFI_RATE_BASIC;
      }

    }
    ptr += 2 + num_xrates;
    actual_length += 2 + num_xrates;
  }

  p->take(max_len - actual_length);
  output(0).push(p);
}



void
BeaconSource::push(int, Packet *p)
{

  uint8_t dir;
  uint8_t type;
  uint8_t subtype;

  if (p->length() < sizeof(struct click_wifi)) {
    click_chatter("%p{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_wifi));

    p->kill();
    return;

  }
  struct click_wifi *w = (struct click_wifi *) p->data();


  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%p{element}: received non-management packet\n",
		  this);
    p->kill();
    return;
  }

  if (subtype != WIFI_FC0_SUBTYPE_PROBE_REQ) {
    click_chatter("%p{element}: received non-probe-req packet\n",
		  this);
    p->kill();
    return;
  }

  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  uint8_t *end  = (uint8_t *) p->data() + p->length();

  uint8_t *ssid_l = NULL;
  uint8_t *rates_l = NULL;

  while (ptr < end) {
    switch (*ptr) {
    case WIFI_ELEMID_SSID:
      ssid_l = ptr;
      break;
    case WIFI_ELEMID_RATES:
      rates_l = ptr;
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

  StringAccum sa;
  String ssid = "";
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, WIFI_MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  }


  /* respond to blank ssid probes also */
  if (ssid != "" && ssid != _winfo->_ssid) {
    if (_debug) {
      click_chatter("%p{element}: other ssid %s wanted %s\n",
		    this,
		    ssid.c_str(),
		    _winfo->_ssid.c_str());
    }
    p->kill();
    return;
  }

  EtherAddress src = EtherAddress(w->i_addr2);

  sa << "ProbeReq: " << src << " ssid " << ssid << " ";

  sa << "rates {";
  if (rates_l) {
    for (int x = 0; x < WIFI_MIN((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
      uint8_t rate = rates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
	sa << " * " << (int) (rate ^ WIFI_RATE_BASIC);
      } else {
	sa << " " << (int) rate;
      }
    }
  }
  sa << " }";

  if (_debug) {
    click_chatter("%p{element}: %s\n",
		  this,
		  sa.take_string().c_str());
  }
  send_beacon(src, true);

  p->kill();
  return;
}

enum {H_DEBUG};

static String
BeaconSource_read_param(Element *e, void *thunk)
{
  BeaconSource *td = (BeaconSource *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}
static int
BeaconSource_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BeaconSource *f = (BeaconSource *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!BoolArg().parse(s, debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
BeaconSource::add_handlers()
{
  add_read_handler("debug", BeaconSource_read_param, H_DEBUG);

  add_write_handler("debug", BeaconSource_write_param, H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BeaconSource)

