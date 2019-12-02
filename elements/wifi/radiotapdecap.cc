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
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <clicknet/radiotap.h>
#include <clicknet/llc.h>
extern "C" {
	#include <click/radiotap_iter.h>
}
CLICK_DECLS

RadiotapDecap::RadiotapDecap() : _debug(false)
{
}

RadiotapDecap::~RadiotapDecap()
{
}

Packet *
RadiotapDecap::simple_action(Packet *p) {

	struct ieee80211_radiotap_header *th = (struct ieee80211_radiotap_header *) p->data();
	struct ieee80211_radiotap_iterator iter;
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	int err = ieee80211_radiotap_iterator_init(&iter, th, p->length(), 0);

	if (err) {
		click_chatter("%{element} :: %s :: malformed radiotap header (init returns %d)", this, __func__, err);
		goto drop;
	}

	memset((void*)ceh, 0, sizeof(struct click_wifi_extra));
	ceh->magic = WIFI_EXTRA_MAGIC;

	while (!(err = ieee80211_radiotap_iterator_next(&iter))) {
		u_int16_t flags;
		switch (iter.this_arg_index) {
		case IEEE80211_RADIOTAP_TSFT:
			ceh->tsft = *((uint64_t *)iter.this_arg);
			break;
		case IEEE80211_RADIOTAP_FLAGS:
			flags = le16_to_cpu(*(uint16_t *)iter.this_arg);
			if (flags & IEEE80211_RADIOTAP_F_DATAPAD) {
				ceh->pad = 1;
			}
			if (flags & IEEE80211_RADIOTAP_F_FCS) {
				p->take(4);
			}
			break;
		case IEEE80211_RADIOTAP_MCS:
			ceh->rate = *((uint8_t *)iter.this_arg+2);
			ceh->flags |= WIFI_EXTRA_MCS;
			break;
		case IEEE80211_RADIOTAP_RATE:
			ceh->rate = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_DATA_RETRIES:
			ceh->max_tries = *iter.this_arg + 1;
			break;
		case IEEE80211_RADIOTAP_CHANNEL:
			ceh->channel = le16_to_cpu(*(uint16_t *)iter.this_arg);
			break;
		case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
			ceh->rssi = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_DBM_ANTNOISE:
			ceh->silence = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
			ceh->rssi = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_DB_ANTNOISE:
			ceh->silence = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_RX_FLAGS:
			flags = le16_to_cpu(*(uint16_t *)iter.this_arg);
			if (flags & IEEE80211_RADIOTAP_F_BADFCS)
				ceh->flags |= WIFI_EXTRA_RX_ERR;
			break;
		case IEEE80211_RADIOTAP_TX_FLAGS:
			flags = le16_to_cpu(*(uint16_t *)iter.this_arg);
			ceh->flags |= WIFI_EXTRA_TX;
			if (flags & IEEE80211_RADIOTAP_F_TX_FAIL)
				ceh->flags |= WIFI_EXTRA_TX_FAIL;
			break;
		}
	}

	if (err != -ENOENT) {
		click_chatter("%{element} :: %s :: malformed radiotap data", this, __func__);
		goto drop;
	}

	p->pull(le16_to_cpu(th->it_len));
	p->set_mac_header(p->data()); // reset mac-header pointer

	return p;

  drop:

	p->kill();
	return 0;

}

CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapDecap)
