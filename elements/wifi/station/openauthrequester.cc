/*
 * openauthrequester.{cc,hh} -- sends 802.11 probe responses from requests.
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
#include "openauthrequester.hh"
#include <elements/wifi/wirelessinfo.hh>

CLICK_DECLS

OpenAuthRequester::OpenAuthRequester()
{
}

OpenAuthRequester::~OpenAuthRequester()
{
}

int
OpenAuthRequester::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (Args(conf, this, errh)
      .read("DEBUG", _debug)
      .read("ETH", _eth)
      .read("WIRELESS_INFO", ElementCastArg("WirelessInfo"), _winfo)
      .complete() < 0)
    return -1;

  return 0;
}

void
OpenAuthRequester::push(int, Packet *p)
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

  if (subtype != WIFI_FC0_SUBTYPE_AUTH) {
    click_chatter("%p{element}: received non-probe-req packet\n",
		  this);
    p->kill();
    return;
  }

  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);



  uint16_t algo = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t seq =le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t status =le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;


  EtherAddress src = EtherAddress(w->i_addr2);
  if (algo != WIFI_AUTH_ALG_OPEN) {
    click_chatter("%p{element}: auth %d from %s not supported\n",
		  this,
		  algo,
		  src.unparse().c_str());
    p->kill();
    return;
  }

  if (_debug) {
    click_chatter("%p{element}: auth %d seq %d status %d\n",
		  this,
		  algo,
		  seq,
		  status);
  }


  p->kill();
  return;
}
void
OpenAuthRequester::send_auth_request()
{

  EtherAddress bssid = _winfo ? _winfo->_bssid : EtherAddress();
  int len = sizeof (struct click_wifi) +
    2 +                  /* alg */
    2 +                  /* seq */
    2 +                  /* status */
    0;

  WritablePacket *p = Packet::make(len);

  if (p == 0)
    return;

  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_AUTH;
  w->i_fc[1] = WIFI_FC1_DIR_NODS;

  memcpy(w->i_addr1, bssid.data(), 6);
  memcpy(w->i_addr2, _eth.data(), 6);
  memcpy(w->i_addr3, bssid.data(), 6);


  w->i_dur = 0;
  w->i_seq = 0;

  uint8_t *ptr;

  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  *(uint16_t *)ptr = cpu_to_le16(WIFI_AUTH_ALG_OPEN);
  ptr += 2;

  *(uint16_t *)ptr = cpu_to_le16(1);
  ptr += 2;

  *(uint16_t *)ptr = cpu_to_le16(0);
  ptr += 2;

  output(0).push(p);
}


enum {H_DEBUG, H_ETH, H_SEND_AUTH_REQ};

static String
OpenAuthRequester_read_param(Element *e, void *thunk)
{
  OpenAuthRequester *td = (OpenAuthRequester *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_ETH:
    return td->_eth.unparse() + "\n";
  default:
    return String();
  }
}
static int
OpenAuthRequester_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  OpenAuthRequester *f = (OpenAuthRequester *)e;
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
  case H_SEND_AUTH_REQ: {
    f->send_auth_request();
    break;
  }
  }
  return 0;
}

void
OpenAuthRequester::add_handlers()
{
  add_read_handler("debug", OpenAuthRequester_read_param, H_DEBUG);
  add_read_handler("eth", OpenAuthRequester_read_param, H_ETH);

  add_write_handler("debug", OpenAuthRequester_write_param, H_DEBUG);
  add_write_handler("eth", OpenAuthRequester_write_param, H_ETH);
  add_write_handler("send_auth_req", OpenAuthRequester_write_param, H_SEND_AUTH_REQ);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OpenAuthRequester)
