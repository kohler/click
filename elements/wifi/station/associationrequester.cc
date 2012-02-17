/*
 * associationrequester.{cc,hh} -- decapsultates 802.11 packets
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
#include "associationrequester.hh"

CLICK_DECLS

AssociationRequester::AssociationRequester()
  : _rtable(0),
    _winfo(0)
{
}

AssociationRequester::~AssociationRequester()
{
}

int
AssociationRequester::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _associated = false;
  if (Args(conf, this, errh)
      .read("DEBUG", _debug)
      .read("ETH", _eth)
      .read("WIRELESS_INFO", ElementCastArg("WirelessInfo"), _winfo)
      .read_m("RT", ElementCastArg("AvailableRates"), _rtable)
      .complete() < 0)
    return -1;

  return 0;
}
void
AssociationRequester::send_assoc_req()
{
  EtherAddress bssid = _winfo ? _winfo->_bssid : EtherAddress();
  String ssid = _winfo ? _winfo->_ssid : "";
  int linterval = _winfo ? _winfo->_interval : 1;
  Vector<int> rates = _rtable->lookup(bssid);
  int max_len = sizeof (struct click_wifi) +
    2 + /* cap_info */
    2 + /* listen_int */
    2 + ssid.length() +
    2 + WIFI_RATES_MAXSIZE +  /* rates */
    2 + WIFI_RATES_MAXSIZE +  /* xrates */
    0;


  WritablePacket *p = Packet::make(max_len);

  if(p == 0)
    return;


  if (!rates.size()) {
    click_chatter("%p{element}: couldn't lookup rates for %s\n",
		  this,
		  bssid.unparse().c_str());
  }
  struct click_wifi *w = (struct click_wifi *) p->data();
  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_ASSOC_REQ;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  w->i_dur = 0;
  w->i_seq = 0;

  memcpy(w->i_addr1, bssid.data(), 6);
  memcpy(w->i_addr2, _eth.data(), 6);
  memcpy(w->i_addr3, bssid.data(), 6);

  uint8_t *ptr = (uint8_t *)  p->data() + sizeof(click_wifi);
  int actual_length = sizeof (struct click_wifi);

  uint16_t capability = 0;
  capability |= WIFI_CAPINFO_ESS;
  if (_winfo && _winfo->_wep) {
    capability |= WIFI_CAPINFO_PRIVACY;
  }

  /* capability */
  *(uint16_t *) ptr = cpu_to_le16(capability);
  ptr += 2;
  actual_length += 2;

  /* listen_int */
  *(uint16_t *) ptr = cpu_to_le16(linterval);
  ptr += 2;
  actual_length += 2;

  ptr[0] = WIFI_ELEMID_SSID;
  ptr[1] = ssid.length();
  ptr += 2;
  actual_length += 2;

  memcpy(ptr, ssid.c_str(), ssid.length());
  ptr += ssid.length();
  actual_length += ssid.length();

  /* rates */
  ptr[0] = WIFI_ELEMID_RATES;
  ptr[1] = WIFI_MIN(WIFI_RATE_SIZE, rates.size());
  for (int x = 0; x < WIFI_MIN(WIFI_RATE_SIZE, rates.size()); x++) {
    ptr[2 + x] = (uint8_t) rates[x];

    if (rates[x] == 2) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }

    if (_winfo && _winfo->_channel > 15 && rates[x] == 12) {
      ptr [2 + x] |= WIFI_RATE_BASIC;
    }

  }
  ptr += 2 + WIFI_MIN(WIFI_RATE_SIZE, rates.size());
  actual_length += 2 + WIFI_MIN(WIFI_RATE_SIZE, rates.size());


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
      if (_winfo && _winfo->_channel > 15 && rates[x] == 12) {
	ptr [2 + x] |= WIFI_RATE_BASIC;
      }

    }
    ptr += 2 + num_xrates;
    actual_length += 2 + num_xrates;
  }

  p->take(max_len - actual_length);
  _associated = false;
  output(0).push(p);
}

void
AssociationRequester::process_response(Packet *p)
{
  struct click_wifi *w = (struct click_wifi *) p->data();
  EtherAddress bssid = EtherAddress(w->i_addr3);
  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t status = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t associd = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint8_t *rates_l = ptr;

  Vector<int> basic_rates;
  Vector<int> rates;
  Vector<int> all_rates;
  if (rates_l) {
    for (int x = 0; x < WIFI_MIN((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
      uint8_t rate = rates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
	basic_rates.push_back((int)(rate & WIFI_RATE_VAL));
      } else {
	rates.push_back((int)(rate & WIFI_RATE_VAL));
      }
      all_rates.push_back((int)(rate & WIFI_RATE_VAL));
    }
  }

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  StringAccum sa;
  sa << bssid << " ";
  int rssi = ceh->rssi;
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

  sa << ")";
  if (_debug) {
    click_chatter("%p{element}: response %s\n",
		  this,
		  sa.take_string().c_str());
  }

  if (_rtable) {
    _rtable->insert(bssid, all_rates);
  }

  if (status == 0) {
    _associated = true;
  }
  return;
}

void
AssociationRequester::process_disassociation(Packet *p)
{
  struct click_wifi *w = (struct click_wifi *) p->data();
  uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
  EtherAddress bssid = EtherAddress(w->i_addr3);
  uint16_t reason = le16_to_cpu(*(uint16_t *) ptr);

  ptr += 2;

  if (_winfo && _winfo->_bssid == bssid) {
    click_chatter("%p{element} disassociation from %s reason %d\n",
		  this,
		  bssid.unparse().c_str(),
		  reason);
    _associated = false;
  } else {
    click_chatter("%p{element} BAD disassociation from %s reason %d\n",
		  this,
		  bssid.unparse().c_str(),
		  reason);
  }
  return;
}
void
AssociationRequester::push(int, Packet *p)
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
    return ;

  }

  struct click_wifi *w = (struct click_wifi *) p->data();


  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%p{element}: received non-management packet\n",
		  this);
    p->kill();
    return ;
  }

  if (subtype == WIFI_FC0_SUBTYPE_ASSOC_RESP) {
    process_response(p);
    p->kill();
    return;
  }

  if (subtype == WIFI_FC0_SUBTYPE_DISASSOC) {
    process_disassociation(p);
    p->kill();
    return;
  }


  click_chatter("%p{element}: received non-assoc response packet\n",
		this);
  p->kill();
  return ;

}

enum {H_DEBUG, H_ETH,
      H_SEND_ASSOC_REQ,
      H_ASSOCIATED,
};

static String
AssociationRequester_read_param(Element *e, void *thunk)
{
  AssociationRequester *td = (AssociationRequester *)e;
    switch ((uintptr_t) thunk) {
    case H_DEBUG:
      return String(td->_debug) + "\n";
    case H_ASSOCIATED:
      return String(td->_associated) + "\n";
    case H_ETH:
      return td->_eth.unparse() + "\n";
    default:
      return String();
    }
}
static int
AssociationRequester_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AssociationRequester *f = (AssociationRequester *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!BoolArg().parse(s, debug))
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_ETH: {    //debug
    EtherAddress e;
    if (!cp_ethernet_address(s, &e))
      return errh->error("eth parameter must be ethernet address");
    f->_eth = e;
    break;
  }
  case H_SEND_ASSOC_REQ: {
    f->send_assoc_req();
  }
  }
  return 0;
}

void
AssociationRequester::add_handlers()
  {
  add_read_handler("debug", AssociationRequester_read_param, H_DEBUG);
  add_read_handler("eth", AssociationRequester_read_param, H_ETH);
  add_read_handler("associated", AssociationRequester_read_param, H_ASSOCIATED);


  add_write_handler("debug", AssociationRequester_write_param, H_DEBUG);
  add_write_handler("eth", AssociationRequester_write_param, H_ETH);
  add_write_handler("send_assoc_req", AssociationRequester_write_param, H_SEND_ASSOC_REQ);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(AssociationRequester)
