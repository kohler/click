/*
 * proberesponder.{cc,hh} -- sends 802.11 probe responses from requests.
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
#include <click/error.hh>
#include "proberesponder.hh"
#include <elements/wifi/availablerates.hh>
CLICK_DECLS


#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y))

ProbeResponder::ProbeResponder()
  : Element(1, 1),
    _rtable(0)
{
  MOD_INC_USE_COUNT;
}

ProbeResponder::~ProbeResponder()
{
  MOD_DEC_USE_COUNT;
}

int
ProbeResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _channel = 0;
  _ssid = String();
  _interval_ms = 0;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  "CHANNEL", cpInteger, "channel", &_channel,
		  "SSID", cpString, "ssid", &_ssid,
		  "BSSID", cpEthernetAddress, "bssid", &_bssid,
		  "INTERVAL", cpInteger, "interval_ms", &_interval_ms,
		  "RT", cpElement, "availablerates", &_rtable,
		  0) < 0)
    return -1;


  if (!_rtable || _rtable->cast("AvailableRates") == 0) 
    return errh->error("AvailableRates element is not provided or not a AvailableRates");

  if (_interval_ms <= 0) {
    return errh->error("INTERVAL must be >0\n");
  }

  return 0;
}

void
ProbeResponder::push(int, Packet *p)
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
    return;
	      
  }
  struct click_wifi *w = (struct click_wifi *) p->data();


  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%{element}: received non-management packet\n",
		  this);
    p->kill();
    return;
  }

  if (subtype != WIFI_FC0_SUBTYPE_PROBE_REQ) {
    click_chatter("%{element}: received non-probe-req packet\n",
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
	click_chatter("%{element}: ignored element id %u %u \n",
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
    ssid = String((char *) ssid_l + 2, min((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  }


  /* respond to blank ssid probes also */
  if (ssid != "" && ssid != _ssid) {
    p->kill();
    return;
  }
  
  EtherAddress src = EtherAddress(w->i_addr2);

  sa << "ProbeReq: " << src << " ssid " << ssid << " ";

  sa << "rates {";
  if (rates_l) {
    for (int x = 0; x < min((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
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
    click_chatter("%{element}: %s\n",
		  this,
		  sa.take_string().cc());
  }
  send_probe_response(src);
  
  p->kill();
  return;
}
void
ProbeResponder::send_probe_response(EtherAddress dst)
{

  Vector<int> rates = _rtable->lookup(_bssid);
  int len = sizeof (struct click_wifi) + 
    8 +                  /* timestamp */
    2 +                  /* beacon interval */
    2 +                  /* cap_info */
    2 + _ssid.length() + /* ssid */
    2 + min(WIFI_RATES_MAXSIZE, rates.size()) +  /* rates */
    2 + 1 +              /* ds parms */
    2 + 4 +              /* tim */
    0;
    
  WritablePacket *p = Packet::make(len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_PROBE_RESP;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memcpy(w->i_addr1, dst.data(), 6);
  memcpy(w->i_addr2, _bssid.data(), 6);
  memcpy(w->i_addr3, _bssid.data(), 6);

  
  *(uint16_t *) w->i_dur = 0;
  *(uint16_t *) w->i_seq = 0;

  uint8_t *ptr;
  
  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  /* timestamp is set in the hal. ??? */
  memset(ptr, 0, 8);
  ptr += 8;

  
  uint16_t beacon_int = (uint16_t) _interval_ms;
  *(uint16_t *)ptr = cpu_to_le16(beacon_int);
  ptr += 2;

  uint16_t cap_info = 0;
  cap_info |= WIFI_CAPINFO_ESS;
  *(uint16_t *)ptr = cpu_to_le16(cap_info);
  ptr += 2;

  /* ssid */
  ptr[0] = WIFI_ELEMID_SSID;
  ptr[1] = _ssid.length();
  memcpy(ptr + 2, _ssid.data(), _ssid.length());
  ptr += 2 + _ssid.length();

  /* rates */
  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = min(WIFI_RATES_MAXSIZE, rates.size());
  for (int x = 0; x < min (WIFI_RATES_MAXSIZE, rates.size()); x++) {
    ptr[2 + x] = (uint8_t) rates[x];
    
    if (rates[x] == 2) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }
    
  }
  ptr += 2 + rates.size();

  /* channel */
  ptr[0] = WIFI_ELEMID_DSPARMS;
  ptr[1] = 1;
  ptr[2] = (uint8_t) _channel;
  ptr += 2 + 1;


  /* tim */
  ptr[0] = WIFI_ELEMID_TIM;
  ptr[1] = 4;

  ptr[2] = 0; //count
  ptr[3] = 1; //period
  ptr[4] = 0; //bitmap control
  ptr[5] = 0; //paritial virtual bitmap
  ptr += 2 + 4;

  SET_WIFI_FROM_CLICK(p);
  output(0).push(p);
}


enum {H_DEBUG, H_BSSID, H_SSID, H_CHANNEL, H_INTERVAL};

static String 
ProbeResponder_read_param(Element *e, void *thunk)
{
  ProbeResponder *td = (ProbeResponder *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_BSSID:
    return td->_bssid.s() + "\n";
  case H_SSID:
    return td->_ssid + "\n";
  case H_CHANNEL:
    return String(td->_channel) + "\n";
  case H_INTERVAL:
    return String(td->_interval_ms) + "\n";
  default:
    return String();
  }
}
static int 
ProbeResponder_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  ProbeResponder *f = (ProbeResponder *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_BSSID: {    //debug
    EtherAddress e;
    if (!cp_ethernet_address(s, &e)) 
      return errh->error("bssid parameter must be ethernet address");
    f->_bssid = e;
    break;
  }
  case H_SSID: {    //debug
    f->_ssid = s;
    break;
  }
  case H_CHANNEL: {    //channel
    int channel;
    if (!cp_integer(s, &channel)) 
      return errh->error("channel parameter must be int");
    f->_channel = channel;
    break;
  }
  case H_INTERVAL: {    //mode
    int m;
    if (!cp_integer(s, &m)) 
      return errh->error("interval parameter must be int");
    f->_interval_ms = m;
    break;
  }
  }
  return 0;
}
 
void
ProbeResponder::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", ProbeResponder_read_param, (void *) H_DEBUG);
  add_read_handler("bssid", ProbeResponder_read_param, (void *) H_BSSID);
  add_read_handler("ssid", ProbeResponder_read_param, (void *) H_SSID);
  add_read_handler("channel", ProbeResponder_read_param, (void *) H_CHANNEL);
  add_read_handler("interval", ProbeResponder_read_param, (void *) H_INTERVAL);

  add_write_handler("debug", ProbeResponder_write_param, (void *) H_DEBUG);
  add_write_handler("bssid", ProbeResponder_write_param, (void *) H_BSSID);
  add_write_handler("ssid", ProbeResponder_write_param, (void *) H_SSID);
  add_write_handler("channel", ProbeResponder_write_param, (void *) H_CHANNEL);
  add_write_handler("interval", ProbeResponder_write_param, (void *) H_INTERVAL);
}


#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(ProbeResponder)
