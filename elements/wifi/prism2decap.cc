/*
 * prism2decap.{cc,hh} -- decapsultates 802.11 packets
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
#include "prism2decap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
CLICK_DECLS

Prism2Decap::Prism2Decap()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

Prism2Decap::~Prism2Decap()
{
  MOD_DEC_USE_COUNT;
}

int
Prism2Decap::configure(Vector<String> &conf, ErrorHandler *errh)
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
Prism2Decap::simple_action(Packet *p)
{

  u_int32_t *ptr = (u_int32_t *) p->data();

  if (ptr[0] == DIDmsg_lnxind_wlansniffrm) {
    wlan_ng_prism2_header *ph = (wlan_ng_prism2_header *) p->data();
    SET_WIFI_SIGNAL_ANNO(p, ph->rssi.data);
    SET_WIFI_NOISE_ANNO(p, ph->noise.data);
    SET_WIFI_RATE_ANNO(p, ph->rate.data);    
    p->pull(sizeof(wlan_ng_prism2_header));
  }

  return p;
}


enum {H_DEBUG};

static String 
Prism2Decap_read_param(Element *e, void *thunk)
{
  Prism2Decap *td = (Prism2Decap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
Prism2Decap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  Prism2Decap *f = (Prism2Decap *)e;
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
Prism2Decap::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", Prism2Decap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", Prism2Decap_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(Prism2Decap)
