/*
 * wififragment.{cc,hh} -- Reassembles 802.11 fragments.
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
#include "wififragment.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
CLICK_DECLS


WifiFragment::WifiFragment()
{
}

WifiFragment::~WifiFragment()
{
}

int
WifiFragment::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _max_length = 0;
  if (Args(conf, this, errh)
      .read_p("MTU", _max_length)
      .read("DEBUG", _debug)
      .complete() < 0)
    return -1;
  return 0;
}


void
WifiFragment::push(int port, Packet *p)
{

  click_wifi *w = (click_wifi *) p->data();
  uint16_t seq = le16_to_cpu(w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
  if (!_max_length ||
      p->length() <= sizeof(click_wifi) + _max_length) {
    if (_debug) {
      click_chatter("%p{element}: no modificatoin\n",
		    this);
    }
    output(port).push(p);
    return;
  }

  int num_frags = (p->length() - sizeof(click_wifi))  / _max_length;
  int last_len = (p->length() - sizeof(click_wifi))  % _max_length;
  if (last_len) {
    num_frags++;
  } else {
    last_len = _max_length;
  }
  if (_debug) {
    click_chatter("%p{element} click_wifi %d, seq %d frags %d last_len %d\n",
		  this, sizeof(click_wifi), seq, num_frags, last_len);
  }
  for (int frag = 0; frag < num_frags; frag++) {
    uint32_t frag_len = (frag == num_frags - 1) ? last_len : _max_length;
    if (_debug) {
      click_chatter("%p{element}: %s: on frag %d/%d len %d\n",
		    this,
		    __func__,
		    frag, num_frags,
		    sizeof(click_wifi) + frag_len);
    }

    Packet *p_out = Packet::make(sizeof(click_wifi) + frag_len);
    memcpy((void *) p_out->data(), p->data(), sizeof(click_wifi));

    memcpy((void *) (p_out->data() + sizeof(click_wifi)),
	   p->data() + sizeof(click_wifi) + frag*_max_length,
	   frag_len);
    click_wifi *w_o = (click_wifi *) p_out->data();
    uint16_t seq_o = (seq << WIFI_SEQ_SEQ_SHIFT) | (((u_int8_t) frag) & WIFI_SEQ_FRAG_MASK);
    w_o->i_seq = cpu_to_le16(seq_o);
    if (frag != num_frags - 1) {
      w_o->i_fc[1] |= WIFI_FC1_MORE_FRAG;
    }

    output(port).push(p_out);
  }
  p->kill();
  return;

}


enum {H_DEBUG, };

String
WifiFragment::read_param(Element *e, void *thunk)
{
  WifiFragment *td = (WifiFragment *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
int
WifiFragment::write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WifiFragment *f = (WifiFragment *)e;
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
WifiFragment::add_handlers()
{
  add_read_handler("debug", read_param, H_DEBUG);

  add_write_handler("debug", write_param, H_DEBUG);
}

EXPORT_ELEMENT(WifiFragment)
CLICK_ENDDECLS

