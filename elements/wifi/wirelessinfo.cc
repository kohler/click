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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "wirelessinfo.hh"
#include <click/router.hh>
CLICK_DECLS

WirelessInfo::WirelessInfo()
  : _ssid(""),
    _bssid((const unsigned char *) "\000\000\000\000\000\000"),
    _channel(-1),
    _interval(100),
    _wep(false)
{

}

WirelessInfo::~WirelessInfo()
{
}

int
WirelessInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res;
  reset();
  res = Args(conf, this, errh)
      .read("SSID", _ssid)
      .read("BSSID", _bssid)
      .read("CHANNEL", _channel)
      .read("INTERVAL", _interval)
      .read("WEP", _wep)
#if CLICK_NS
      .read("IFID", _ifid)
#endif
      .complete();

#if CLICK_NS
  // nletor - change interface number ifid
  // to correct channel in simulator
  if (_ifid >= 0)
      simclick_sim_command(router()->simnode(), SIMCLICK_CHANGE_CHANNEL, _ifid, _channel);
#endif

  return res;
}


enum {H_SSID, H_BSSID, H_CHANNEL, H_INTERVAL, H_WEP, H_RESET};


void
WirelessInfo::reset()
{
  _ssid = "";
  _channel = -1;
  _bssid = EtherAddress();
  _interval = 100;
  _wep = false;
#if CLICK_NS
  _ifid = -1;
#endif
}
int
WirelessInfo::write_param(const String &in_s, Element *e, void *vparam,
			  ErrorHandler *errh)
{
  WirelessInfo *f = (WirelessInfo *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
 case H_SSID: {
    f->_ssid = s;
    break;
  }
  case H_BSSID: {
    EtherAddress e;
    if (!EtherAddressArg().parse(s, e))
      return errh->error("bssid parameter must be ethernet address");
    f->_bssid = e;
    break;
  }

  case H_CHANNEL: {
    int m;
    if (!IntArg().parse(s, m))
      return errh->error("channel parameter must be int");
    f->_channel = m;
#if CLICK_NS
    if (f->_ifid >= 0)
	simclick_sim_command(f->router()->simnode(), SIMCLICK_CHANGE_CHANNEL, f->_ifid, f->_channel);
#endif
    break;
  }
 case H_INTERVAL: {
    int m;
    if (!IntArg().parse(s, m))
      return errh->error("interval parameter must be int");
    f->_interval = m;
    break;
 }
  case H_WEP: {    //debug
    bool wep;
    if (!BoolArg().parse(s, wep))
      return errh->error("wep parameter must be boolean");
    f->_wep = wep;
    break;
  }
  case H_RESET: f->reset(); break;
  }
  return 0;
}

String
WirelessInfo::read_param(Element *e, void *thunk)
{
  WirelessInfo *td = (WirelessInfo *)e;
    switch ((uintptr_t) thunk) {
    case H_SSID: return td->_ssid + "\n";
    case H_BSSID: return td->_bssid.unparse() + "\n";
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
  add_read_handler("ssid", read_param, H_SSID);
  add_read_handler("bssid", read_param, H_BSSID);
  add_read_handler("channel", read_param, H_CHANNEL);
  add_read_handler("interval", read_param, H_INTERVAL);
  add_read_handler("wep", read_param, H_WEP);


  add_write_handler("ssid", write_param, H_SSID);
  add_write_handler("bssid", write_param, H_BSSID);
  add_write_handler("channel", write_param, H_CHANNEL);
  add_write_handler("interval", write_param, H_INTERVAL);
  add_write_handler("wep", write_param, H_WEP);
  add_write_handler("reset", write_param, H_RESET, Handler::BUTTON);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(WirelessInfo)

