/*
 * wifidefrag.{cc,hh} -- Reassembles 802.11 fragments.
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
#include "wifidefrag.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
CLICK_DECLS


WifiDefrag::WifiDefrag()
{
}

WifiDefrag::~WifiDefrag()
{
}

int
WifiDefrag::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _debug = false;
    return Args(conf, this, errh).read("DEBUG", _debug).complete();
}

Packet *
WifiDefrag::simple_action(Packet *p)
{

  click_wifi *w = (click_wifi *) p->data();
  EtherAddress src = EtherAddress(w->i_addr2);
  uint16_t seq = le16_to_cpu(w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
  uint8_t frag = le16_to_cpu(w->i_seq) & WIFI_SEQ_FRAG_MASK;
  u_int8_t more_frag = w->i_fc[1] & WIFI_FC1_MORE_FRAG;
  PacketInfo *nfo = _packets.findp(src);


  if (!more_frag && frag == 0) {
    /* default case: no defrag needed */
    if (_debug) {
      click_chatter("%p{element}: no defrag %s seq %d frag %d\n",
		    this,
		    src.unparse().c_str(),
		    seq,
		    frag);
    }
    if (nfo && nfo->next_frag) {
      nfo->clear();
    }
    return p;
  }

  if (frag && (!nfo || nfo->next_frag != frag ||
	       (nfo->seq && nfo->seq != seq))) {
    /* unrelated fragment */
    if (_debug) {
      click_chatter("%p{element}: unrelated frag %s seq %d frag %d\n",
		    this,
		    src.unparse().c_str(),
		    seq,
		    frag);
      if (nfo) {
	click_chatter("nfo seq %d next_frag %d\n",
		      nfo->seq,
		      nfo->next_frag);
      }
    }
    if (nfo) {
      nfo->clear();
    }
    p->kill();
    return 0;
  }

  if (!nfo) {
    _packets.insert(src, PacketInfo(src));
    nfo = _packets.findp(src);
  }
  assert(nfo);

  if (frag == 0) {
    /* first frag */
    nfo->p = p;
    nfo->seq = seq;
    if (_debug) {
      click_chatter("%p{element}: first frag %s seq %d frag %d\n",
		    this,
		    src.unparse().c_str(),
		    seq,
		    frag);
    }

  } else {
    /* copy frag to other packet */
    assert(nfo->p);
    uint32_t len = nfo->p->length();

    p->pull(sizeof(click_wifi));
    if ((nfo->p = nfo->p->put(p->length())))
	memcpy((void *) (nfo->p->data() + len), p->data(), p->length());
    p->kill();
  }

  if (more_frag) {
    nfo->next_frag++;
    return 0;
  }

  if (_debug) {
    click_chatter("%p{element}: last frag %s seq %d frag %d\n",
		  this,
		  src.unparse().c_str(),
		  seq,
		  frag);
  }
  p = nfo->p;
  w = (click_wifi *) p->data();
  w->i_seq = cpu_to_le16(((u_int16_t) nfo->seq) << WIFI_SEQ_SEQ_SHIFT);
  w->i_fc[1] ^= WIFI_FC1_MORE_FRAG;

  nfo->p = 0;
  nfo->clear();



  //_packets.remove(src);
  return p;
}


enum {H_DEBUG, };

String
WifiDefrag::read_param(Element *e, void *thunk)
{
  WifiDefrag *td = (WifiDefrag *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
int
WifiDefrag::write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WifiDefrag *f = (WifiDefrag *)e;
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
WifiDefrag::add_handlers()
{
  add_read_handler("debug", read_param, H_DEBUG);

  add_write_handler("debug", write_param, H_DEBUG);
}

EXPORT_ELEMENT(WifiDefrag)
CLICK_ENDDECLS

