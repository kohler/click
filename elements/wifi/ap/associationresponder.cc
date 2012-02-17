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
#include <click/args.hh>
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
#include <elements/wifi/wirelessinfo.hh>
CLICK_DECLS

AssociationResponder::AssociationResponder()
  : _associd(0),
    _rtable(0),
    _winfo(0)
{
}

AssociationResponder::~AssociationResponder()
{
}

int
AssociationResponder::configure(Vector<String> &conf, ErrorHandler *errh)
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


void
AssociationResponder::recv_association_request(Packet *p)
{
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


  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%p{element}: received non-management packet\n",
		  this);
    p->kill();
    return;
  }

  if (subtype != WIFI_FC0_SUBTYPE_ASSOC_REQ) {
    click_chatter("%p{element}: received non-association-req packet\n",
		  this);
    p->kill();
    return;
  }

  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  /*capabilty */
  uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  /* listen interval */
  uint16_t lint = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

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


  String ssid;
  String my_ssid = _winfo ? _winfo->_ssid : "";
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, WIFI_MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  } else {
    /* there was no element or it has zero length */
    ssid = "";
  }


    /* respond to blank ssid probes also */
  if (ssid != "" && ssid != my_ssid) {
    if (_debug) {
      click_chatter("%p{element}: other ssid %s wanted %s\n",
		    this,
		    ssid.c_str(),
		    my_ssid.c_str());
    }
    p->kill();
    return;
  }

  StringAccum sa;


  EtherAddress dst = EtherAddress(w->i_addr1);
  EtherAddress src = EtherAddress(w->i_addr2);
  EtherAddress bssid = EtherAddress(w->i_addr3);

  sa << "src " << src;
  sa << " dst " << dst;
  sa << " bssid " << bssid;
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

  sa << " listen_int " << lint << " ";

  sa << "( { ";
  for (int x = 0; x < basic_rates.size(); x++) {
    sa << basic_rates[x] << " ";
  }
  sa << "} ";
  for (int x = 0; x < rates.size(); x++) {
    sa << rates[x] << " ";
  }

  sa << ")\n";

  click_chatter("%p{element}: request %s\n",
		this,
		sa.take_string().c_str());



  uint16_t associd = 0xc000 | _associd++;

  if (_debug) {
    click_chatter("%p{element}: association %s associd %d\n",
		  this,
		  src.unparse().c_str(),
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
  EtherAddress bssid = _winfo ? _winfo->_bssid : EtherAddress();
  Vector<int> rates = _rtable->lookup(bssid);
  int max_len = sizeof (struct click_wifi) +
    2 +                  /* cap_info */
    2 +                  /* status  */
    2 +                  /* assoc_id */
    2 + WIFI_RATES_MAXSIZE +  /* rates */
    2 + WIFI_RATES_MAXSIZE +  /* xrates */
    0;

  WritablePacket *p = Packet::make(max_len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_ASSOC_RESP;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memcpy(w->i_addr1, dst.data(), 6);
  memcpy(w->i_addr2, bssid.data(), 6);
  memcpy(w->i_addr3, bssid.data(), 6);


  w->i_dur = 0;
  w->i_seq = 0;

  uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);
  int actual_length = sizeof(struct click_wifi);

  uint16_t cap_info = 0;
  cap_info |= WIFI_CAPINFO_ESS;
  *(uint16_t *)ptr = cpu_to_le16(cap_info);
  ptr += 2;
  actual_length += 2;

  *(uint16_t *)ptr = cpu_to_le16(status);
  ptr += 2;
  actual_length += 2;

  *(uint16_t *)ptr = cpu_to_le16(associd);
  ptr += 2;
  actual_length += 2;


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
AssociationResponder::send_disassociation(EtherAddress dst, uint16_t reason)
{

  EtherAddress bssid = _winfo ? _winfo->_bssid : EtherAddress();
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
  memcpy(w->i_addr2, bssid.data(), 6);
  memcpy(w->i_addr3, bssid.data(), 6);


  w->i_dur = 0;
  w->i_seq = 0;


  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  *(uint16_t *)ptr = cpu_to_le16(reason);
  ptr += 2;

  output(0).push(p);
}


enum {H_DEBUG};

static String
AssociationResponder_read_param(Element *e, void *thunk)
{
  AssociationResponder *td = (AssociationResponder *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
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
AssociationResponder::add_handlers()
{
  add_read_handler("debug", AssociationResponder_read_param, H_DEBUG);

  add_write_handler("debug", AssociationResponder_write_param, H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AssociationResponder)
