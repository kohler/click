/*
 * wifiencap.{cc,hh} -- encapsultates 802.11 packets
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
#include "wifiencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include <elements/wifi/wirelessinfo.hh>
CLICK_DECLS


WifiEncap::WifiEncap()
  : Element(1, 1),
    _winfo(0)
{
}

WifiEncap::~WifiEncap()
{
}

int
WifiEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _mode = WIFI_FC1_DIR_NODS;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "mode", &_mode, 
		  cpOptional,
		  cpEthernetAddress, "bssid", &_bssid,
		  /* not required */
		  cpKeywords,
		  "WIRELESS_INFO", cpElement, "wirleess_info", &_winfo,
		  "DEBUG", cpBool, "Debug", &_debug,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
WifiEncap::simple_action(Packet *p)
{


  EtherAddress src;
  EtherAddress dst;
  EtherAddress bssid = _winfo ? _winfo->_bssid : _bssid;

  uint16_t ethtype;
  WritablePacket *p_out = 0;

  if (p->length() < sizeof(struct click_ether)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_ether));

    p->kill();
    return 0;
	      
  }

  click_ether *eh = (click_ether *) p->data();
  src = EtherAddress(eh->ether_shost);
  dst = EtherAddress(eh->ether_dhost);
  ethtype = eh->ether_type;


  p_out = p->uniqueify();
  if (!p_out) {
    return 0;
  }


  p_out->pull(sizeof(struct click_ether));
  p_out = p_out->push(sizeof(struct click_llc));

  if (!p_out) {
    return 0;
  }

  struct click_llc *llc = (struct click_llc *) p_out->data();
  llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
  llc->llc_control = LLC_UI;
  llc->llc_un.type_snap.org_code[0] = 0;
  llc->llc_un.type_snap.org_code[1] = 0;
  llc->llc_un.type_snap.org_code[2] = 0;
  llc->llc_un.type_snap.ether_type = ethtype;

  p_out->push(sizeof(struct click_wifi));
  struct click_wifi *w = (struct click_wifi *) p_out->data();

  memset(p_out->data(), 0, sizeof(click_wifi));
  w->i_fc[0] = (uint8_t) (WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_DATA);
  w->i_fc[1] = 0;
  w->i_fc[1] |= (uint8_t) (WIFI_FC1_DIR_MASK & _mode);



  switch (_mode) {
  case WIFI_FC1_DIR_NODS:
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, bssid.data(), 6);
    break;
  case WIFI_FC1_DIR_TODS:
    memcpy(w->i_addr1, bssid.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, dst.data(), 6);
    break;
  case WIFI_FC1_DIR_FROMDS:
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, bssid.data(), 6);
    memcpy(w->i_addr3, src.data(), 6);
    break;
  case WIFI_FC1_DIR_DSTODS:
    /* XXX this is wrong */
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, bssid.data(), 6);
    break;
  default:
    click_chatter("%{element}: invalid mode %d\n",
		  this,
		  _mode);
    p_out->kill();
    return 0;
  }
  return p_out;
}


enum {H_DEBUG, H_MODE, H_BSSID};

static String 
WifiEncap_read_param(Element *e, void *thunk)
{
  WifiEncap *td = (WifiEncap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
      case H_MODE:
	return String(td->_mode) + "\n";
      case H_BSSID:
	return td->_bssid.s() + "\n";
    default:
      return String();
    }
}
static int 
WifiEncap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WifiEncap *f = (WifiEncap *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }

  case H_MODE: {    //mode
    int m;
    if (!cp_integer(s, &m)) 
      return errh->error("mode parameter must be int");
    f->_mode = m;
    break;
  }
  case H_BSSID: {    //debug
    EtherAddress e;
    if (!cp_ethernet_address(s, &e)) 
      return errh->error("bssid parameter must be ethernet address");
    f->_bssid = e;
    break;
  }
  }
  return 0;
}
 
void
WifiEncap::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", WifiEncap_read_param, (void *) H_DEBUG);
  add_read_handler("mode", WifiEncap_read_param, (void *) H_MODE);
  add_read_handler("bssid", WifiEncap_read_param, (void *) H_BSSID);

  add_write_handler("debug", WifiEncap_write_param, (void *) H_DEBUG);
  add_write_handler("mode", WifiEncap_write_param, (void *) H_MODE);
  add_write_handler("bssid", WifiEncap_write_param, (void *) H_BSSID);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(WifiEncap)
