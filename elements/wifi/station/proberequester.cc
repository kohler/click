/*
 * proberequester.{cc,hh} -- sends 802.11 probe responses from requests.
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
#include "proberequester.hh"
#include <elements/wifi/availablerates.hh>

CLICK_DECLS


#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y))


ProbeRequester::ProbeRequester()
  : Element(0, 1),
    _rtable(0)  
{
  MOD_INC_USE_COUNT;
}

ProbeRequester::~ProbeRequester()
{
  MOD_DEC_USE_COUNT;
}

int
ProbeRequester::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _ssid = "";
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  "SSID", cpString, "ssid", &_ssid,
		  "ETH", cpEthernetAddress, "bssid", &_eth,
		  "RT", cpElement, "availablerates", &_rtable,
		  cpEnd) < 0)
    return -1;

  if (!_rtable || _rtable->cast("AvailableRates") == 0) 
    return errh->error("AvailableRates element is not provided or not a AvailableRates");

  return 0;
}

void
ProbeRequester::send_probe_request()
{
  Vector<int> rates = _rtable->lookup(_eth);
  int max_len = sizeof (struct click_wifi) + 
    2 + _ssid.length() + /* ssid */
    2 + WIFI_RATE_SIZE + /* rates */
    2 + WIFI_RATE_SIZE + /* xrates */
    0;
    
  WritablePacket *p = Packet::make(max_len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_PROBE_REQ;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memset(w->i_addr1, 0xff, 6);
  memcpy(w->i_addr2, _eth.data(), 6);
  memset(w->i_addr3, 0xff, 6);

  
  *(uint16_t *) w->i_dur = 0;
  *(uint16_t *) w->i_seq = 0;

  uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
  int actual_length = sizeof (struct click_wifi);

  /* ssid */
  ptr[0] = WIFI_ELEMID_SSID;
  ptr[1] = _ssid.length();
  memcpy(ptr + 2, _ssid.data(), _ssid.length());
  ptr += 2 + _ssid.length();
  actual_length += 2 + _ssid.length();

    /* rates */
  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = min(WIFI_RATE_SIZE, rates.size());
  for (int x = 0; x < min (WIFI_RATE_SIZE, rates.size()); x++) {
    ptr[2 + x] = (uint8_t) rates[x];
    
    if (rates[x] == 2) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }
    
  }
  ptr += 2 + min(WIFI_RATE_SIZE, rates.size());
  actual_length += 2 + min(WIFI_RATE_SIZE, rates.size());


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


enum {H_DEBUG, H_ETH, H_SSID, H_SEND_PROBE};

static String 
ProbeRequester_read_param(Element *e, void *thunk)
{
  ProbeRequester *td = (ProbeRequester *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_ETH:
    return td->_eth.s() + "\n";
  case H_SSID:
    return td->_ssid + "\n";
  default:
    return String();
  }
}
static int 
ProbeRequester_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  ProbeRequester *f = (ProbeRequester *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_ETH: {
    EtherAddress e;
    if (!cp_ethernet_address(s, &e)) 
      return errh->error("bssid parameter must be ethernet address");
    f->_eth = e;
    break;
  }
  case H_SSID: {
    f->_ssid = s;
    break;
  }
  case H_SEND_PROBE: {
    f->send_probe_request();
  }
  }
  return 0;
}
 
void
ProbeRequester::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", ProbeRequester_read_param, (void *) H_DEBUG);
  add_read_handler("eth", ProbeRequester_read_param, (void *) H_ETH);
  add_read_handler("ssid", ProbeRequester_read_param, (void *) H_SSID);

  add_write_handler("debug", ProbeRequester_write_param, (void *) H_DEBUG);
  add_write_handler("eth", ProbeRequester_write_param, (void *) H_ETH);
  add_write_handler("ssid", ProbeRequester_write_param, (void *) H_SSID);
  add_write_handler("send_probe", ProbeRequester_write_param, (void *) H_SEND_PROBE);
}


#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(ProbeRequester)
