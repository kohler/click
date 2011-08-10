/*
 * settxrate.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include <click/packet_anno.hh>
#include "settxrate.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

SetTXRate::SetTXRate()
{
}

SetTXRate::~SetTXRate()
{
}

int
SetTXRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _rate = 0;
  _et = 0;
  _offset = 0;
  _tries = WIFI_MAX_RETRIES+1;
  if (Args(conf, this, errh)
      .read_p("RATE", _rate)
      .read("TRIES", _tries)
      .read("ETHTYPE", _et)
      .read("OFFSET", _offset)
      .complete() < 0) {
    return -1;
  }

  if (_rate < 0) {
	  return errh->error("RATE must be >= 0");
  }

  if (_tries < 1) {
	  return errh->error("TRIES must be >= 0");
  }


  return 0;
}

Packet *
SetTXRate::simple_action(Packet *p_in)
{
  uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
  click_ether *eh = (click_ether *) dst_ptr;

  if (_et && eh->ether_type != htons(_et)) {
    return p_in;
  }

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
  ceh->magic = WIFI_EXTRA_MAGIC;
  ceh->rate = _rate ? _rate : 2;
  ceh->max_tries = _tries;

  return p_in;
}
enum {H_RATE, H_TRIES};

String
SetTXRate::read_handler(Element *e, void *thunk)
{
  SetTXRate *foo = (SetTXRate *)e;
  switch((uintptr_t) thunk) {
  case H_RATE: return String(foo->_rate) + "\n";
  case H_TRIES: return String(foo->_tries) + "\n";
  default:   return "\n";
  }

}

int
SetTXRate::write_handler(const String &arg, Element *e,
			 void *vparam, ErrorHandler *errh)
{
  SetTXRate *f = (SetTXRate *) e;
  String s = cp_uncomment(arg);
  switch((intptr_t)vparam) {
  case H_RATE: {
    unsigned m;
    if (!IntArg().parse(s, m))
      return errh->error("rate parameter must be unsigned");
    f->_rate = m;
    break;
  }
  case H_TRIES: {
    unsigned m;
    if (!IntArg().parse(s, m))
      return errh->error("tries parameter must be unsigned");
    f->_tries = m;
    break;
  }
  }
  return 0;
}

void
SetTXRate::add_handlers()
{
  add_read_handler("rate", read_handler, H_RATE);
  add_read_handler("tries", read_handler, H_TRIES);
  add_write_handler("rate", write_handler, H_RATE);
  add_write_handler("tries", write_handler, H_TRIES);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXRate)

