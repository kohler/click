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
#include <click/confparse.hh>
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

CLICK_DECLS

BeaconSource::BeaconSource()
  : Element(0, 1),
    _timer(this),
    _rtable(0)
{
  MOD_INC_USE_COUNT;
}

BeaconSource::~BeaconSource()
{
  MOD_DEC_USE_COUNT;
}

int
BeaconSource::configure(Vector<String> &conf, ErrorHandler *errh)
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

int
BeaconSource::initialize (ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(_interval_ms);
  return 0;
}

void
BeaconSource::run_timer() 
{
  send_beacon();
  _timer.schedule_after_ms(_interval_ms);
}

void
BeaconSource::send_beacon()
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

  uint8_t dir;
  uint8_t type;
  uint8_t subtype;

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_BEACON;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memset(w->i_addr1, 0xff, 6);
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
BeaconSource_read_param(Element *e, void *thunk)
{
  BeaconSource *td = (BeaconSource *)e;
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
BeaconSource_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BeaconSource *f = (BeaconSource *)e;
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
BeaconSource::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", BeaconSource_read_param, (void *) H_DEBUG);
  add_read_handler("bssid", BeaconSource_read_param, (void *) H_BSSID);
  add_read_handler("ssid", BeaconSource_read_param, (void *) H_SSID);
  add_read_handler("channel", BeaconSource_read_param, (void *) H_CHANNEL);
  add_read_handler("interval", BeaconSource_read_param, (void *) H_INTERVAL);

  add_write_handler("debug", BeaconSource_write_param, (void *) H_DEBUG);
  add_write_handler("bssid", BeaconSource_write_param, (void *) H_BSSID);
  add_write_handler("ssid", BeaconSource_write_param, (void *) H_SSID);
  add_write_handler("channel", BeaconSource_write_param, (void *) H_CHANNEL);
  add_write_handler("interval", BeaconSource_write_param, (void *) H_INTERVAL);
}


#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(BeaconSource)
