/*
 * wifistation.{cc,hh} -- decapsultates 802.11 packets
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
#include "wifistation.hh"

CLICK_DECLS

WifiStation::WifiStation()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

WifiStation::~WifiStation()
{
  MOD_DEC_USE_COUNT;
}

int
WifiStation::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_parse(conf, this, errh,
		  cpEthernetAddress, "eth", &_eth,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  0) < 0)
    return -1;
  return 0;
}
void
WifiStation::send_assoc_req()
{
  int len = sizeof (struct click_wifi) + 
    2 + /* cap_info */
    2 + /* listen_int */
    6 + /* current_ap */
    2 + _ssid.length() +
    2 + min(8, _rates.size());
    
    
  WritablePacket *p = Packet::make(len);
  if(p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();
  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_ASSOC_REQ;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  w->i_dur[0] = 0;
  w->i_dur[1] = 0;

  w->i_seq[0] = 0;
  w->i_seq[1] = 0;

  memcpy(w->i_addr1, _bssid.data(), 6);
  memcpy(w->i_addr2, _eth.data(), 6);
  memcpy(w->i_addr3, _bssid.data(), 6);

  uint8_t *ptr = (uint8_t *)  p->data() + sizeof(click_wifi);

  uint16_t capability = 0;
  capability |= WIFI_CAPINFO_ESS;

  /* capability */
  *(uint16_t *) ptr = cpu_to_le16(capability);
  ptr += 2;

  /* listen_int */
  ptr[0] = 0;
  ptr[1] = 1;
  ptr += 2;

  ptr[0] = WIFI_ELEMID_SSID;
  ptr[1] = _ssid.length();
  ptr += 2;

  memcpy(ptr, _ssid.cc(), _ssid.length());
  ptr += _ssid.length();


  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = min(8, _rates.size());

  ptr += 2;
  for (int x = 0; x < min(8, _rates.size()); x++) {
    ptr[x] = _rates[x];
    if (1 || ptr[x] == 2) {
      ptr[x] |= WIFI_RATE_BASIC;
    }
  }
  

  SET_WIFI_FROM_CLICK(p);
  output(0).push(p);
}
void
WifiStation::push(int port, Packet *p)
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
    return ;
	      
  }

  struct click_wifi *w = (struct click_wifi *) p->data();


  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%{element}: received non-management packet\n",
		  this);
    p->kill();
    return ;
  }

  if (subtype != WIFI_FC0_SUBTYPE_ASSOC_RESP) {
    click_chatter("%{element}: received non-assoc response packet\n",
		  this);
    p->kill();
    return ;
  }

  EtherAddress bssid = EtherAddress(w->i_addr3);
  uint8_t *ptr;
  
  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  uint8_t *cap_l = ptr;
  ptr += 2;
  uint16_t capability = cap_l[0] + (cap_l[1] << 8);


  uint8_t *status_l = ptr;
  ptr += 2;
  uint16_t status = status_l[0] + (status_l[1] << 8);


  uint8_t *associd_l = ptr;
  ptr += 2;
  uint16_t associd = associd_l[0] + (associd_l[1] << 8);
  
  uint8_t *rates_l = ptr;
  
  Vector<int> basic_rates;
  Vector<int> rates;
  if (rates_l) {
    for (int x = 0; x < min((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
      uint8_t rate = rates_l[x + 2];
      
      if (rate & WIFI_RATE_BASIC) {
	basic_rates.push_back((int)(rate & WIFI_RATE_VAL));
      } else {
	rates.push_back((int)(rate & WIFI_RATE_VAL));
      }
    }
  }

  StringAccum sa;
  sa << bssid << " ";
  int rssi = WIFI_SIGNAL_ANNO(p);
  sa << "+" << rssi << " ";
  
  sa << "[ ";
  if (capability & WIFI_CAPINFO_ESS) {
    sa << "ESS ";
  }
  if (capability & WIFI_CAPINFO_IBSS) {
    sa << "IBSS ";
  }
  if (capability & WIFI_CAPINFO_CF_POLLABLE) {
    sa << "CF_POLLABLE ";
  }
  if (capability & WIFI_CAPINFO_CF_POLLREQ) {
    sa << "CF_POLLREQ ";
  }
  if (capability & WIFI_CAPINFO_PRIVACY) {
    sa << "PRIVACY ";
  }
  sa << "] ";

  sa << "status " << status << " ";
  sa << "associd " << associd << " ";

  sa << "( { ";
  for (int x = 0; x < basic_rates.size(); x++) {
    sa << basic_rates[x] << " ";
  }
  sa << "} ";
  for (int x = 0; x < rates.size(); x++) {
    sa << rates[x] << " ";
  }

  sa << ")\n";

  click_chatter("%{element}: response %s\n",
		this,
		sa.take_string().cc());

  p->kill();
  return;
}

enum {H_DEBUG, H_BSSID, H_ETH, H_SSID, H_LISTEN_INTERVAL, H_RATES, H_CLEAR_RATES, H_RUN};

static String 
WifiStation_read_param(Element *e, void *thunk)
{
  WifiStation *td = (WifiStation *)e;
    switch ((uintptr_t) thunk) {
    case H_DEBUG:
      return String(td->_debug) + "\n";
    case H_BSSID:
      return td->_bssid.s() + "\n";
    case H_ETH:
      return td->_eth.s() + "\n";
    case H_SSID:
      return td->_ssid + "\n";
    case H_LISTEN_INTERVAL:
      return String(td->_listen_interval) + "\n";
    case H_RATES: {
      String s;
      for (int x = 0; x < td->_rates.size(); x++) {
	s += String(td->_rates[x]) + " ";
      }
      return s + "\n";
    }
    default:
      return String();
    }
}
static int 
WifiStation_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WifiStation *f = (WifiStation *)e;
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
  case H_ETH: {    //debug
    EtherAddress e;
    if (!cp_ethernet_address(s, &e)) 
      return errh->error("eth parameter must be ethernet address");
    f->_eth = e;
    break;
  }
  case H_SSID: {    //debug
    f->_ssid = s;
    break;
  }
  case H_LISTEN_INTERVAL: {    
    int m;
    if (!cp_integer(s, &m)) 
      return errh->error("listen_interval parameter must be int");
    f->_listen_interval = m;
    break;
  }
  case H_RATES: {    //mode
    int m;
    if (!cp_integer(s, &m)) 
      return errh->error("rate parameter must be int");
    f->_rates.push_back(m);
    break;
  }
  case H_CLEAR_RATES: {
    f->_rates.clear();
  }
  case H_RUN: {
    f->send_assoc_req();
  }
  }
  return 0;
}
 
void
WifiStation::add_handlers()
  {
  add_default_handlers(true);

  add_read_handler("debug", WifiStation_read_param, (void *) H_DEBUG);
  add_read_handler("bssid", WifiStation_read_param, (void *) H_BSSID);
  add_read_handler("eth", WifiStation_read_param, (void *) H_ETH);
  add_read_handler("ssid", WifiStation_read_param, (void *) H_SSID);
  add_read_handler("listen_interval", WifiStation_read_param, (void *) H_LISTEN_INTERVAL);
  add_read_handler("rates", WifiStation_read_param, (void *) H_RATES);


  add_write_handler("debug", WifiStation_write_param, (void *) H_DEBUG);
  add_write_handler("bssid", WifiStation_write_param, (void *) H_BSSID);
  add_write_handler("eth", WifiStation_write_param, (void *) H_ETH);
  add_write_handler("ssid", WifiStation_write_param, (void *) H_SSID);
  add_write_handler("listen_interval", WifiStation_write_param, (void *) H_LISTEN_INTERVAL);
  add_write_handler("rates", WifiStation_write_param, (void *) H_RATES);
  add_write_handler("rates_reset", WifiStation_write_param, (void *) H_CLEAR_RATES);
  add_write_handler("run", WifiStation_write_param, (void *) H_RUN);

}


#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(WifiStation)
