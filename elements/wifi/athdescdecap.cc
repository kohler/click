/*
 * athdescdecap.{cc,hh} -- decapsultates 802.11 packets
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
#include "athdescdecap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include "athdesc.h"
CLICK_DECLS

AthdescDecap::AthdescDecap()
{
    static_assert(WIFI_EXTRA_ANNO_SIZE >= sizeof(click_wifi_extra), "WIFI_EXTRA_ANNO_SIZE is not big enough.");
}

AthdescDecap::~AthdescDecap()
{
}

int
AthdescDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _debug = false;
    return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
AthdescDecap::simple_action(Packet *p)
{
	WritablePacket *q = p->uniqueify();
	if (q) {
		struct ar5212_desc *desc = (struct ar5212_desc *) (q->data() + 8);
		click_wifi_extra *eh = WIFI_EXTRA_ANNO(q);
		memset(eh, 0, sizeof(click_wifi_extra));
		eh->magic = WIFI_EXTRA_MAGIC;
		if (desc->frame_len == 0) {
			struct ar5212_rx_status *rx_desc = (struct ar5212_rx_status *) (q->data() + 16);
			/* rx */
			eh->rate = ratecode_to_dot11(rx_desc->rx_rate);
			eh->rssi = rx_desc->rx_rssi;
			if (!rx_desc->rx_ok) {
				eh->flags |= WIFI_EXTRA_RX_ERR;
			}
		} else {
			eh->flags |= WIFI_EXTRA_TX;
			/* tx */
			eh->power = desc->xmit_power;
			eh->rssi = desc->ack_sig_strength;
			eh->rate = ratecode_to_dot11(desc->xmit_rate0);
			eh->max_tries = desc->data_fail_count;
			if (desc->excessive_retries)
				eh->flags |= WIFI_EXTRA_TX_FAIL;
		}
		q->pull(ATHDESC_HEADER_SIZE);
	}

  return q;
}


enum {H_DEBUG};

static String
AthdescDecap_read_param(Element *e, void *thunk)
{
  AthdescDecap *td = (AthdescDecap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int
AthdescDecap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AthdescDecap *f = (AthdescDecap *)e;
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
AthdescDecap::add_handlers()
{
	add_read_handler("debug", AthdescDecap_read_param, H_DEBUG);
	add_write_handler("debug", AthdescDecap_write_param, H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(AthdescDecap)
