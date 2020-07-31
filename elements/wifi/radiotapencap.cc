/*
 * radiotapencap.{cc,hh} -- encapsultates 802.11 packets
 * John Bicket, Roberto Riggio
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
#include "radiotapencap.hh"
#include <click/etheraddress.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include <clicknet/radiotap.h>
CLICK_DECLS

#define CLICK_RADIOTAP_PRESENT (			          \
	(1 << IEEE80211_RADIOTAP_RATE)			        | \
	(1 << IEEE80211_RADIOTAP_TX_FLAGS)    		    | \
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)		    | \
	(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE)	| \
	(1 << IEEE80211_RADIOTAP_EXT)			        | \
	0)

#define CLICK_RADIOTAP_PRESENT_HT (			          \
	(1 << IEEE80211_RADIOTAP_TX_FLAGS)    		    | \
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)		    | \
	(1 << IEEE80211_RADIOTAP_MCS)			        | \
	(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE)	| \
	(1 << IEEE80211_RADIOTAP_EXT)			        | \
	0)

struct click_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	u_int32_t	it_present1;
	u_int32_t	it_present2;
	u_int32_t	it_present3;

	u_int8_t	wt_rate;
	u_int8_t	wt_pad;
	u_int16_t	wt_tx_flags;
	u_int8_t	wt_data_retries;

	u_int8_t	wt_rate1;
	u_int8_t	wt_data_retries1;

	u_int8_t	wt_rate2;
	u_int8_t	wt_data_retries2;

	u_int8_t	wt_rate3;
	u_int8_t	wt_data_retries3;

} __attribute__((__packed__));

struct click_radiotap_header_ht {
	struct ieee80211_radiotap_header wt_ihdr;
	u_int32_t	it_present1;
	u_int32_t	it_present2;
	u_int32_t	it_present3;

	u_int16_t	wt_tx_flags;
	u_int8_t	wt_data_retries;
	u_int8_t	wt_known;
	u_int8_t	wt_flags;
	u_int8_t	wt_mcs;

	u_int8_t	wt_data_retries1;
	u_int8_t	wt_known1;
	u_int8_t	wt_flags1;
	u_int8_t	wt_mcs1;

	u_int8_t	wt_data_retries2;
	u_int8_t	wt_known2;
	u_int8_t	wt_flags2;
	u_int8_t	wt_mcs2;

	u_int8_t	wt_data_retries3;
	u_int8_t	wt_known3;
	u_int8_t	wt_flags3;
	u_int8_t	wt_mcs3;
} __attribute__((__packed__));

RadiotapEncap::RadiotapEncap() : _debug(false) {
}

RadiotapEncap::~RadiotapEncap() {
}

Packet *
RadiotapEncap::simple_action(Packet *p) {
	click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	if (ceh->flags & WIFI_EXTRA_MCS)
		return encap_ht(p);
	return encap(p);
}

Packet *
RadiotapEncap::encap_ht(Packet *p) {

	click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	WritablePacket *p_out = p->uniqueify();

	if (!p_out) {
		p->kill();
		return 0;
	}

	p_out = p_out->push(sizeof(struct click_radiotap_header_ht));

	if (!p_out) {
		p->kill();
		return 0;
	}

	struct click_radiotap_header_ht *crh  = (struct click_radiotap_header_ht *) p_out->data();

	memset(crh, 0, sizeof(struct click_radiotap_header_ht));

	crh->wt_ihdr.it_version = 0;
	crh->wt_ihdr.it_len = cpu_to_le16(sizeof(struct click_radiotap_header_ht));

	crh->wt_ihdr.it_present = cpu_to_le32(CLICK_RADIOTAP_PRESENT_HT);

	crh->wt_known |= IEEE80211_RADIOTAP_MCS_HAVE_BW |
	                 IEEE80211_RADIOTAP_MCS_HAVE_MCS |
	                 IEEE80211_RADIOTAP_MCS_HAVE_GI;

	if (ceh->flags & WIFI_EXTRA_MCS_SGI) {
		crh->wt_flags |= IEEE80211_RADIOTAP_MCS_SGI;
	}

	if (ceh->flags & WIFI_EXTRA_MCS_BW_40) {
		crh->wt_flags |= IEEE80211_RADIOTAP_MCS_BW_40;
	}

	crh->wt_mcs = ceh->rate;
	crh->wt_data_retries = (ceh->max_tries > 0) ? ceh->max_tries - 1 : WIFI_MAX_RETRIES;

	if (ceh->flags & WIFI_EXTRA_TX_NOACK) {
		crh->wt_tx_flags |= IEEE80211_RADIOTAP_F_TX_NOACK;
	}

	crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE);
	crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_EXT);
	crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE);
	crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_EXT);

	if (ceh->rate1 != -1) {
		crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_MCS);
		crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
		crh->wt_mcs1 = ceh->rate1;
		crh->wt_data_retries1 = (ceh->max_tries1 > 0) ? ceh->max_tries1 - 1 : WIFI_MAX_RETRIES;
		crh->wt_known1 |= IEEE80211_RADIOTAP_MCS_HAVE_BW |
		                  IEEE80211_RADIOTAP_MCS_HAVE_MCS |
		                  IEEE80211_RADIOTAP_MCS_HAVE_GI;
		if (ceh->flags & WIFI_EXTRA_MCS_SGI) {
			crh->wt_flags1 |= IEEE80211_RADIOTAP_MCS_SGI;
		}

		if (ceh->flags & WIFI_EXTRA_MCS_BW_40) {
			crh->wt_flags1 |= IEEE80211_RADIOTAP_MCS_BW_40;
		}
		if (ceh->rate2 != -1) {
			crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_MCS);
			crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
			crh->wt_mcs2 = ceh->rate2;
			crh->wt_data_retries2 = (ceh->max_tries2 > 0) ? ceh->max_tries2 - 1 : WIFI_MAX_RETRIES;
			crh->wt_known2 |= IEEE80211_RADIOTAP_MCS_HAVE_BW |
			                  IEEE80211_RADIOTAP_MCS_HAVE_MCS |
			                  IEEE80211_RADIOTAP_MCS_HAVE_GI;
			if (ceh->flags & WIFI_EXTRA_MCS_SGI) {
				crh->wt_flags2 |= IEEE80211_RADIOTAP_MCS_SGI;
			}

			if (ceh->flags & WIFI_EXTRA_MCS_BW_40) {
				crh->wt_flags2 |= IEEE80211_RADIOTAP_MCS_BW_40;
			}
			if (ceh->rate3 != -1) {
				crh->it_present3 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_MCS);
				crh->it_present3 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
				crh->wt_mcs3 = ceh->rate3;
				crh->wt_data_retries3 = (ceh->max_tries3 > 0) ? ceh->max_tries3 - 1 : WIFI_MAX_RETRIES;
				crh->wt_known3 |= IEEE80211_RADIOTAP_MCS_HAVE_BW |
				                  IEEE80211_RADIOTAP_MCS_HAVE_MCS |
				                  IEEE80211_RADIOTAP_MCS_HAVE_GI;
				if (ceh->flags & WIFI_EXTRA_MCS_SGI) {
					crh->wt_flags3 |= IEEE80211_RADIOTAP_MCS_SGI;
				}

				if (ceh->flags & WIFI_EXTRA_MCS_BW_40) {
					crh->wt_flags3 |= IEEE80211_RADIOTAP_MCS_BW_40;
				}
			}
		}
	}

	return p_out;

}

Packet *
RadiotapEncap::encap(Packet *p) {

	click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	WritablePacket *p_out = p->uniqueify();

	if (!p_out) {
		p->kill();
		return 0;
	}

	p_out = p_out->push(sizeof(struct click_radiotap_header));

	if (!p_out) {
		p->kill();
		return 0;
	}

	struct click_radiotap_header *crh  = (struct click_radiotap_header *) p_out->data();

	memset(crh, 0, sizeof(struct click_radiotap_header));

	crh->wt_ihdr.it_version = 0;
	crh->wt_ihdr.it_len = cpu_to_le16(sizeof(struct click_radiotap_header));

	crh->wt_ihdr.it_present = cpu_to_le32(CLICK_RADIOTAP_PRESENT);

	crh->wt_rate = ceh->rate;
	crh->wt_data_retries = (ceh->max_tries > 0) ? ceh->max_tries - 1 : WIFI_MAX_RETRIES;

	if (ceh->flags & WIFI_EXTRA_TX_NOACK) {
		crh->wt_tx_flags |= IEEE80211_RADIOTAP_F_TX_NOACK;
	}

	crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE);
	crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_EXT);
	crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE);
	crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_EXT);

	if (ceh->rate1 != -1) {
		crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
		crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
		crh->wt_rate1 = ceh->rate1;
		crh->wt_data_retries1 = (ceh->max_tries1 > 0) ? ceh->max_tries1 - 1 : WIFI_MAX_RETRIES;
		if (ceh->rate2 != -1) {
			crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
			crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
			crh->wt_rate2 = ceh->rate2;
			crh->wt_data_retries2 = (ceh->max_tries2 > 0) ? ceh->max_tries2 - 1 : WIFI_MAX_RETRIES;
			if (ceh->rate3 != -1) {
				crh->it_present3 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
				crh->it_present3 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
				crh->wt_rate3 = ceh->rate3;
				crh->wt_data_retries3 = (ceh->max_tries3 > 0) ? ceh->max_tries3 - 1 : WIFI_MAX_RETRIES;
			}
		}
	}

	return p_out;

}

CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapEncap)
