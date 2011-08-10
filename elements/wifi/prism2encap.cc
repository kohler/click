/*
 * prism2encap.{cc,hh} -- encapsultates 802.11 packets
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
#include "prism2encap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
CLICK_DECLS

Prism2Encap::Prism2Encap()
{
}

Prism2Encap::~Prism2Encap()
{
}

int
Prism2Encap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _debug = false;
    return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
Prism2Encap::simple_action(Packet *p)
{

  WritablePacket *p_out = p->uniqueify();
  if (!p_out) {
    p->kill();
    return 0;
  }

  p_out = p_out->push(sizeof(wlan_ng_prism2_header));
  if (p_out) {
	  wlan_ng_prism2_header *ph = (wlan_ng_prism2_header *) p_out->data();
	  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	  memset(ph, 0, sizeof(wlan_ng_prism2_header));
	  ph->msgcode = DIDmsg_lnxind_wlansniffrm;
	  ph->msglen = sizeof(wlan_ng_prism2_header);
	  //strcpy(ph->devname, dev->name);

	  ph->hosttime.did = DIDmsg_lnxind_wlansniffrm_hosttime;
	  ph->hosttime.status = 0;
	  ph->hosttime.len = 4;
	  ph->hosttime.data = 0;

	  /* Pass up tsf clock in mactime */
	  ph->mactime.did = DIDmsg_lnxind_wlansniffrm_mactime;
	  ph->mactime.status = 0;
	  ph->mactime.len = 4;
	  ph->mactime.data = 0;

	  ph->istx.did = DIDmsg_lnxind_wlansniffrm_istx;
	  ph->istx.status = 0;
	  ph->istx.len = 4;
	  ph->istx.data = 1;

	  ph->frmlen.did = DIDmsg_lnxind_wlansniffrm_frmlen;
	  ph->frmlen.status = 0;
	  ph->frmlen.len = 4;
	  ph->frmlen.data = p_out->length() - sizeof(wlan_ng_prism2_header);

	  ph->channel.did = DIDmsg_lnxind_wlansniffrm_channel;
	  ph->channel.status = 0;
	  ph->channel.len = 4;
	  ph->channel.data = 0;

	  ph->rssi.did = DIDmsg_lnxind_wlansniffrm_rssi;
	  ph->rssi.status = P80211ENUM_msgitem_status_no_value;
	  ph->rssi.len = 4;
	  ph->rssi.data = 0;

	  ph->signal.did = DIDmsg_lnxind_wlansniffrm_signal;
	  ph->signal.status = 0;
	  ph->signal.len = 4;
	  ph->signal.data = ceh->rssi;

	  ph->rate.did = DIDmsg_lnxind_wlansniffrm_rate;
	  ph->rate.status = 0;
	  ph->rate.len = 4;
	  ph->rate.data = ceh->rate;

  }
  return p_out;
}


enum {H_DEBUG};

static String
Prism2Encap_read_param(Element *e, void *thunk)
{
  Prism2Encap *td = (Prism2Encap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int
Prism2Encap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  Prism2Encap *f = (Prism2Encap *)e;
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
Prism2Encap::add_handlers()
{
  add_read_handler("debug", Prism2Encap_read_param, H_DEBUG);

  add_write_handler("debug", Prism2Encap_write_param, H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(Prism2Encap)
