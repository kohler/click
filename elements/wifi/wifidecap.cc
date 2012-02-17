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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
CLICK_DECLS

WifiDecap::WifiDecap()
{
}

WifiDecap::~WifiDecap()
{
}

int
WifiDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _strict = false;
  _push_eth = true;
  if (Args(conf, this, errh)
      .read("DEBUG", _debug)
      .read("STRICT", _strict)
      .read("ETHER", _push_eth)
      .complete() < 0)
    return -1;
  return 0;
}

Packet *
WifiDecap::simple_action(Packet *p)
{


  uint8_t dir;
  struct click_wifi *w = (struct click_wifi *) p->data();
  EtherAddress bssid;
  EtherAddress src;
  EtherAddress dst;

  int wifi_header_size = sizeof(struct click_wifi);
  if ((w->i_fc[1] & WIFI_FC1_DIR_MASK) == WIFI_FC1_DIR_DSTODS)
	  wifi_header_size += WIFI_ADDR_LEN;
  if (WIFI_QOS_HAS_SEQ(w))
	  wifi_header_size += sizeof(uint16_t);

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  if ((ceh->magic == WIFI_EXTRA_MAGIC) && ceh->pad && (wifi_header_size & 3))
	  wifi_header_size += 4 - (wifi_header_size & 3);

  if (p->length() < wifi_header_size + sizeof(struct click_llc)) {
    p->kill();
    return 0;
  }

  if (w->i_fc[1] & WIFI_FC1_WEP) {
    p->kill();
    return 0;
  }

  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;

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
    click_chatter("%p{element}: invalid dir %d\n",
		  this,
		  dir);
    if (_strict) {
      p->kill();
      return 0;
    }
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
  }

  WritablePacket *p_out = p->uniqueify();
  if (!p_out) {
    return 0;
  }

  uint16_t ether_type;
  if (!_strict || memcmp(WIFI_LLC_HEADER, p_out->data() + wifi_header_size,
			 WIFI_LLC_HEADER_LEN)) {
	  memcpy(&ether_type, p_out->data() + wifi_header_size + sizeof(click_llc) - 2, 2);
  } else {
	  p_out->kill();
	  return 0;
  }

  p_out->pull(wifi_header_size + sizeof(struct click_llc));

  if (_push_eth) {
	  p_out = p_out->push_mac_header(14);
	  if (!p_out) {
	    return 0;
	  }

	  memcpy(p_out->data(), dst.data(), 6);
	  memcpy(p_out->data() + 6, src.data(), 6);
	  memcpy(p_out->data() + 12, &ether_type, 2);

	  if (_debug) {
		  click_chatter("%p{element}: dir %d src %s dst %s bssid %s eth 0x%02x\n",
				this,
				dir,
				src.unparse().c_str(),
				dst.unparse().c_str(),
				bssid.unparse().c_str(),
				ether_type);
	  }
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
WifiDecap::add_handlers()
{
  add_read_handler("debug", WifiDecap_read_param, H_DEBUG);

  add_write_handler("debug", WifiDecap_write_param, H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(WifiDecap)
