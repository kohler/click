/*
 * associationresponder.{cc,hh} -- sends 802.11 association responses from requests.
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
#include "associationresponder.hh"
#include <elements/wifi/availablerates.hh>
CLICK_DECLS

#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y))

AssociationResponder::AssociationResponder()
  : Element(1, 1),
    _associd(0),
    _rtable(0)
  
{
  MOD_INC_USE_COUNT;
}

AssociationResponder::~AssociationResponder()
{
  MOD_DEC_USE_COUNT;
}

int
AssociationResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _ssid = String();
  _interval_ms = 0;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  "SSID", cpString, "ssid", &_ssid,
		  "BSSID", cpEthernetAddress, "bssid", &_bssid,
		  "INTERVAL", cpInteger, "interval_ms", &_interval_ms,
		  "RT", cpElement, "availablerates", &_rtable,
		  cpEnd) < 0)
    return -1;


  if (!_rtable || _rtable->cast("AvailableRates") == 0) 
    return errh->error("AvailableRates element is not provided or not a AvailableRates");

  if (_interval_ms <= 0) {
    return errh->error("INTERVAL must be >0\n");
  }
  return 0;
}


void 
AssociationResponder::recv_association_request(Packet *p)
{
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


  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%{element}: received non-management packet\n",
		  this);
    p->kill();
    return;
  }

  if (subtype != WIFI_FC0_SUBTYPE_ASSOC_REQ) {
    click_chatter("%{element}: received non-association-req packet\n",
		  this);
    p->kill();
    return;
  }

  uint8_t *ptr;
  
  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

    //uint8_t *cap_info = ptr;
  ptr += 2;

  //uint16_t capability = cap_info[0] + (cap_info[1] << 8);


  //uint8_t *lint_info = ptr;
  ptr += 2;

  //uint16_t listen_interval = lint_info[0] + (lint_info[1] << 8);





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

  String ssid;
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, min((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  } else {
    /* there was no element or it has zero length */
    ssid = "";
  }


  if (ssid != _ssid) {
    p->kill();
    return;
  }


  EtherAddress src = EtherAddress(w->i_addr2);
  uint16_t associd = 0xc000 | _associd++;

  if (_debug) {
    click_chatter("%{element}: association %s associd %d\n",
		  this,
		  src.s().cc(),
		  associd);		  
  }
  send_association_response(src, WIFI_STATUS_SUCCESS, associd);
  p->kill();
  return;
}


void
AssociationResponder::push(int, Packet *p)
{

  recv_association_request(p);
}
void
AssociationResponder::send_association_response(EtherAddress dst, uint16_t status, uint16_t associd)
{

  Vector<int> rates = _rtable->lookup(_bssid);
  int len = sizeof (struct click_wifi) + 
    2 +                  /* cap_info */
    2 +                  /* status  */
    2 +                  /* assoc_id */
    2 + min(8, rates.size()) +  /* rates */
    0;
    
  WritablePacket *p = Packet::make(len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_ASSOC_RESP;
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

  
  uint16_t cap_info = 0;
  cap_info |= WIFI_CAPINFO_ESS;
  *(uint16_t *)ptr = cpu_to_le16(cap_info);
  ptr += 2;

  *(uint16_t *)ptr = cpu_to_le16(status);
  ptr += 2;


  *(uint16_t *)ptr = cpu_to_le16(associd);
  ptr += 2;


  /* rates */
  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = min(8, rates.size());
  for (int x = 0; x < min (8, rates.size()); x++) {
    ptr[2 + x] = (uint8_t) rates[x];
    
    if (rates[x] == 2) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }
    
  }
  ptr += 2 + rates.size();

  SET_WIFI_FROM_CLICK(p);
  output(0).push(p);
}


void
AssociationResponder::send_disassociation(EtherAddress dst, uint16_t reason)
{

  int len = sizeof (struct click_wifi) + 
    2 +                  /* reason */
    0;
    
  WritablePacket *p = Packet::make(len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_ASSOC_RESP;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memcpy(w->i_addr1, dst.data(), 6);
  memcpy(w->i_addr2, _bssid.data(), 6);
  memcpy(w->i_addr3, _bssid.data(), 6);

  
  *(uint16_t *) w->i_dur = 0;
  *(uint16_t *) w->i_seq = 0;


  uint8_t *ptr;
  
  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  *(uint16_t *)ptr = cpu_to_le16(reason);
  ptr += 2;

  SET_WIFI_FROM_CLICK(p);
  output(0).push(p);
}


enum {H_DEBUG, H_BSSID, H_SSID, H_INTERVAL};

static String 
AssociationResponder_read_param(Element *e, void *thunk)
{
  AssociationResponder *td = (AssociationResponder *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_BSSID:
    return td->_bssid.s() + "\n";
  case H_SSID:
    return td->_ssid + "\n";
  case H_INTERVAL:
    return String(td->_interval_ms) + "\n";
  default:
    return String();
  }
}
static int 
AssociationResponder_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AssociationResponder *f = (AssociationResponder *)e;
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
AssociationResponder::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", AssociationResponder_read_param, (void *) H_DEBUG);
  add_read_handler("bssid", AssociationResponder_read_param, (void *) H_BSSID);
  add_read_handler("ssid", AssociationResponder_read_param, (void *) H_SSID);
  add_read_handler("interval", AssociationResponder_read_param, (void *) H_INTERVAL);

  add_write_handler("debug", AssociationResponder_write_param, (void *) H_DEBUG);
  add_write_handler("bssid", AssociationResponder_write_param, (void *) H_BSSID);
  add_write_handler("ssid", AssociationResponder_write_param, (void *) H_SSID);
  add_write_handler("interval", AssociationResponder_write_param, (void *) H_INTERVAL);
}


#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(AssociationResponder)
