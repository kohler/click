/*
 * radiotapdecap.{cc,hh} -- decapsultates 802.11 packets
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
#include "radiotapdecap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/radiotap.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
CLICK_DECLS

#define NUM_RADIOTAP_ELEMENTS 19

static const int radiotap_elem_to_bytes[NUM_RADIOTAP_ELEMENTS] =
	{8, /* IEEE80211_RADIOTAP_TSFT */
	 1, /* IEEE80211_RADIOTAP_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RATE */
	 2, /* IEEE80211_RADIOTAP_CHANNEL (frequency) */
	 2, /* IEEE80211_RADIOTAP_CHANNEL (flags) */
	 2, /* IEEE80211_RADIOTAP_FHSS */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DBM_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_LOCK_QUALITY */
	 2, /* IEEE80211_RADIOTAP_TX_ATTENUATION */
	 2, /* IEEE80211_RADIOTAP_DB_TX_ATTENUATION */
	 1, /* IEEE80211_RADIOTAP_DBM_TX_POWER */
	 1, /* IEEE80211_RADIOTAP_ANTENNA */
	 1, /* IEEE80211_RADIOTAP_DB_ANTSIGNAL */
	 1, /* IEEE80211_RADIOTAP_DB_ANTNOISE */
	 2, /* IEEE80211_RADIOTAP_RX_FLAGS */
	 2, /* IEEE80211_RADIOTAP_TX_FLAGS */
	 1, /* IEEE80211_RADIOTAP_RTS_RETRIES */
	 1, /* IEEE80211_RADIOTAP_DATA_RETRIES */
	};

static int rt_el_present(struct ieee80211_radiotap_header *th, u_int32_t element)
{
	if (element > NUM_RADIOTAP_ELEMENTS)
		return 0;
	return le32_to_cpu(th->it_present) & (1 << element);
}

static int rt_check_header(struct ieee80211_radiotap_header *th, int len, u_int8_t *offsets[])
{
	int bytes = 0;
	int x = 0;
	u_int8_t *ptr = (u_int8_t *)(th + 1);

	if (th->it_version != 0) {
		return 0;
	}

	if (le16_to_cpu(th->it_len) < sizeof(struct ieee80211_radiotap_header)) {
		return 0;
	}

	for (x = 0; x < NUM_RADIOTAP_ELEMENTS; x++) {
		if (rt_el_present(th, x)) {
		    int pad = bytes % radiotap_elem_to_bytes[x];
		    if (pad)
			bytes += radiotap_elem_to_bytes[x] - pad;
		    offsets[x] = ptr + bytes;
		    bytes += radiotap_elem_to_bytes[x];
		}
	}

	if (le16_to_cpu(th->it_len) < sizeof(struct ieee80211_radiotap_header) + bytes) {
		return 0;
	}

	if (le16_to_cpu(th->it_len) > len) {
		return 0;
	}

	return 1;
}

RadiotapDecap::RadiotapDecap()
{
}

RadiotapDecap::~RadiotapDecap()
{
}

int
RadiotapDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _debug = false;
    return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
RadiotapDecap::simple_action(Packet *p)
{
	u_int8_t *offsets[NUM_RADIOTAP_ELEMENTS];
	struct ieee80211_radiotap_header *th = (struct ieee80211_radiotap_header *) p->data();
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	if (rt_check_header(th, p->length(), offsets)) {
		memset((void*)ceh, 0, sizeof(struct click_wifi_extra));
		ceh->magic = WIFI_EXTRA_MAGIC;

		if (rt_el_present(th, IEEE80211_RADIOTAP_FLAGS)) {
			u_int8_t flags = *offsets[IEEE80211_RADIOTAP_FLAGS];
			if (flags & IEEE80211_RADIOTAP_F_DATAPAD) {
				ceh->pad = 1;
			}
			if (flags & IEEE80211_RADIOTAP_F_FCS) {
				p->take(4);
			}
		}

		if (rt_el_present(th, IEEE80211_RADIOTAP_RATE)) {
			ceh->rate = *offsets[IEEE80211_RADIOTAP_RATE];
		}

		if (rt_el_present(th, IEEE80211_RADIOTAP_DBM_ANTSIGNAL))
			ceh->rssi = *offsets[IEEE80211_RADIOTAP_DBM_ANTSIGNAL];

		if (rt_el_present(th, IEEE80211_RADIOTAP_DBM_ANTNOISE))
			ceh->silence = *offsets[IEEE80211_RADIOTAP_DBM_ANTNOISE];

		if (rt_el_present(th, IEEE80211_RADIOTAP_DB_ANTSIGNAL))
			ceh->rssi = *offsets[IEEE80211_RADIOTAP_DB_ANTSIGNAL];

		if (rt_el_present(th, IEEE80211_RADIOTAP_DB_ANTNOISE))
			ceh->silence = *offsets[IEEE80211_RADIOTAP_DB_ANTNOISE];

		if (rt_el_present(th, IEEE80211_RADIOTAP_RX_FLAGS)) {
			u_int16_t flags = le16_to_cpu(*((u_int16_t *) offsets[IEEE80211_RADIOTAP_RX_FLAGS]));
			if (flags & IEEE80211_RADIOTAP_F_RX_BADFCS)
				ceh->flags |= WIFI_EXTRA_RX_ERR;
		}

		if (rt_el_present(th, IEEE80211_RADIOTAP_TX_FLAGS)) {
			u_int16_t flags = le16_to_cpu(*((u_int16_t *) offsets[IEEE80211_RADIOTAP_TX_FLAGS]));
			ceh->flags |= WIFI_EXTRA_TX;
			if (flags & IEEE80211_RADIOTAP_F_TX_FAIL)
				ceh->flags |= WIFI_EXTRA_TX_FAIL;
		}

		if (rt_el_present(th, IEEE80211_RADIOTAP_DATA_RETRIES))
			ceh->retries = *offsets[IEEE80211_RADIOTAP_DATA_RETRIES];

		p->pull(le16_to_cpu(th->it_len));
		p->set_mac_header(p->data());  // reset mac-header pointer
	}

  return p;
}


enum {H_DEBUG};

static String
RadiotapDecap_read_param(Element *e, void *thunk)
{
  RadiotapDecap *td = (RadiotapDecap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int
RadiotapDecap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  RadiotapDecap *f = (RadiotapDecap *)e;
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
RadiotapDecap::add_handlers()
{
  add_read_handler("debug", RadiotapDecap_read_param, H_DEBUG);

  add_write_handler("debug", RadiotapDecap_write_param, H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapDecap)
