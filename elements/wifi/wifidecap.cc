/*
 * wifidecap.{cc,hh} -- decapsultates 802.11 packets
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
#include "wifidecap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
CLICK_DECLS

WifiDecap::WifiDecap()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

WifiDecap::~WifiDecap()
{
  MOD_DEC_USE_COUNT;
}

int
WifiDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
WifiDecap::simple_action(Packet *p)
{


  uint8_t dir;
  uint8_t type;
  uint8_t subtype;


  if (p->length() < sizeof(struct click_wifi)) {
    if (0) {
      click_chatter("%{element}: packet too small: %d vs %d\n",
		    this,
		    p->length(),
		    sizeof(struct click_wifi));
    }
      
    p->kill();
    return 0;
	      
  }
  struct click_wifi *w = (struct click_wifi *) p->data();

  EtherAddress bssid;
  EtherAddress src;
  EtherAddress dst;

  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;


  switch (dir) {
  case WIFI_FC1_DIR_NODS:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    break;
  case WIFI_FC1_DIR_TODS:
    bssid = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    dst = EtherAddress(w->i_addr3);
    break;
  case WIFI_FC1_DIR_FROMDS:
    dst = EtherAddress(w->i_addr1);
    bssid = EtherAddress(w->i_addr2);
    src = EtherAddress(w->i_addr3);
    break;
  case WIFI_FC1_DIR_DSTODS:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    break;
  default:
    click_chatter("%{element}: invalid dir %d\n",
		  this,
		  dir);
    p->kill();
    return 0;
  }

  WritablePacket *p_out = p->uniqueify();
  if (!p_out) {
    p->kill();
    return 0;
  }

  p_out->pull(sizeof(click_wifi));


  struct click_llc *llc = (struct click_llc *) p_out->data();

  uint16_t ether_type;
  if (llc->llc_dsap == LLC_SNAP_LSAP && llc->llc_ssap == LLC_SNAP_LSAP &&
      llc->llc_un.type_u.control == LLC_UI && llc->llc_un.type_snap.org_code[0] == 0 &&
      llc->llc_un.type_snap.org_code[1] == 0 && llc->llc_un.type_snap.org_code[2] == 0) {
    ether_type = llc->llc_un.type_snap.ether_type;
    p_out->pull(sizeof(struct click_llc));
  }


  p_out->push_mac_header(14);
  memcpy(p_out->data(), dst.data(), 6);
  memcpy(p_out->data() + 6, src.data(), 6);
  memcpy(p_out->data() + 12, &ether_type, 2);

  if (_debug) {
    click_chatter("%{element}: dir %d src %s dst %s bssid %s\n",
		  this,
		  dir,
		  src.s().cc(),
		  dst.s().cc(),
		  bssid.s().cc());
  }

  return p_out;
}


enum {H_DEBUG};

static String 
WifiDecap_read_param(Element *e, void *thunk)
{
  WifiDecap *td = (WifiDecap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
WifiDecap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WifiDecap *f = (WifiDecap *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}
 
void
WifiDecap::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", WifiDecap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", WifiDecap_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(WifiDecap)
