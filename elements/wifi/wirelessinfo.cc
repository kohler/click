/*
 * wirelessinfo.{cc,hh} -- Poor man's arp table
 * John Bicket
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "wirelessinfo.hh"
CLICK_DECLS

WirelessInfo::WirelessInfo()
  : Element(0, 0),
    _ssid(""),
    _bssid((const unsigned char *) "\000\000\000\000\000\000"),
    _channel(-1),
    _interval(100),
    _wep(false)
{
  MOD_INC_USE_COUNT;

}

WirelessInfo::~WirelessInfo()
{
  MOD_DEC_USE_COUNT;
}

int
WirelessInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res;
  res = cp_va_parse(conf, this, errh,
		    cpKeywords, 
		    "SSID", cpString, "ssid", &_ssid,
		    "BSSID", cpEthernetAddress, "bssid", &_bssid,
		    "CHANNEL", cpInteger, "channel", &_channel,
		    "INTERVAL", cpInteger, "interval", &_interval,
		    "WEP", cpBool, "wep", &_wep,
		    cpEnd);

  return res;
}


enum {H_SSID, H_BSSID, H_CHANNEL, H_INTERVAL, H_WEP};

int 
WirelessInfo::write_param(const String &in_s, Element *e, void *vparam,
			  ErrorHandler *errh)
{
  WirelessInfo *f = (WirelessInfo *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
 case H_SSID: {
   String ssid;
    if (!cp_string(s, &ssid)) 
      return errh->error("ssid must be string");
    f->_ssid = ssid;
    break;
  }
  case H_BSSID: { 
    EtherAddress e;
    if (!cp_ethernet_address(s, &e)) 
      return errh->error("bssid parameter must be ethernet address");
    f->_bssid = e;
    break;
  }

  case H_CHANNEL: {
    int m;
    if (!cp_integer(s, &m)) 
      return errh->error("channel parameter must be int");
    f->_channel = m;
    break;
  }
 case H_INTERVAL: {
    int m;
    if (!cp_integer(s, &m)) 
      return errh->error("interval parameter must be int");
    f->_interval = m;
    break;
 }
  case H_WEP: {    //debug
    bool wep;
    if (!cp_bool(s, &wep)) 
      return errh->error("wep parameter must be boolean");
    f->_wep = wep;
    break;
  }
  }
  return 0;
}

String 
WirelessInfo::read_param(Element *e, void *thunk)
{
  WirelessInfo *td = (WirelessInfo *)e;
    switch ((uintptr_t) thunk) {
    case H_SSID: return td->_ssid + "\n";
    case H_BSSID: return td->_bssid.s() + "\n";
    case H_CHANNEL: return String(td->_channel) + "\n";
    case H_INTERVAL: return String(td->_interval) + "\n";
    case H_WEP: return String(td->_wep) + "\n";
    default:
      return "\n";
    }
}

void
WirelessInfo::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("ssid", read_param, (void *) H_SSID);
  add_read_handler("bssid", read_param, (void *) H_BSSID);
  add_read_handler("channel", read_param, (void *) H_CHANNEL);
  add_read_handler("interval", read_param, (void *) H_INTERVAL);
  add_read_handler("wep", read_param, (void *) H_WEP);


  add_write_handler("ssid", write_param, (void *) H_SSID);
  add_write_handler("bssid", write_param, (void *) H_BSSID);
  add_write_handler("channel", write_param, (void *) H_CHANNEL);
  add_write_handler("interval", write_param, (void *) H_INTERVAL);
  add_write_handler("wep", write_param, (void *) H_WEP);
  
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WirelessInfo)

