/*
 * setrts.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "setrts.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

SetRTS::SetRTS()
{
}

SetRTS::~SetRTS()
{
}

int
SetRTS::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("RTS", _rts).complete();
}

Packet *
SetRTS::simple_action(Packet *p)
{

  if (p) {
    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
    ceh->magic = WIFI_EXTRA_MAGIC;
    if (_rts) {
      ceh->flags |= WIFI_EXTRA_DO_RTS_CTS;
    } else {
      ceh->flags &= ~WIFI_EXTRA_DO_RTS_CTS;
    }
  }
  return p;
}

enum {H_RTS};
static String
SetRTS_read_param(Element *e, void *thunk)
{
  SetRTS *td = (SetRTS *)e;
  switch ((uintptr_t) thunk) {
  case H_RTS:
    return String(td->_rts) + "\n";
  default:
    return String();
  }

}

static int
SetRTS_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SetRTS *f = (SetRTS *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_RTS: {
    unsigned m;
    if (!IntArg().parse(s, m))
      return errh->error("stepup parameter must be unsigned");
    f->_rts = m;
    break;
  }
  }
    return 0;

}
void
SetRTS::add_handlers()
{
  add_read_handler("rts", SetRTS_read_param, H_RTS);
  add_write_handler("rts", SetRTS_write_param, H_RTS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetRTS)
