/*
 * settxpower.{cc,hh} -- sets wifi txpower annotation on a packet
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
#include "settxpower.hh"
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
CLICK_DECLS

SetTXPower::SetTXPower()
{
}

SetTXPower::~SetTXPower()
{
}

int
SetTXPower::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _power = 0;
    return Args(conf, this, errh).read_p("POWER", _power).complete();
}

Packet *
SetTXPower::simple_action(Packet *p_in)
{
  if (p_in) {
    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
    ceh->magic = WIFI_EXTRA_MAGIC;
    ceh->power = _power;
    return p_in;
  }
  return 0;
}

enum {H_POWER,

};
static String
SetTXPower_read_param(Element *e, void *thunk)
{
  SetTXPower *td = (SetTXPower *)e;
  switch ((uintptr_t) thunk) {
  case H_POWER:
    return String(td->_power) + "\n";
  default:
    return String();
  }
}
static int
SetTXPower_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SetTXPower *f = (SetTXPower *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_POWER: {
    unsigned m;
    if (!IntArg().parse(s, m))
      return errh->error("power parameter must be unsigned");
    f->_power = m;
    break;

  }
  }
  return 0;
}
void
SetTXPower::add_handlers()
  {
  add_read_handler("power", SetTXPower_read_param, H_POWER);
  add_write_handler("power", SetTXPower_write_param, H_POWER);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXPower)

