/*
 * settxpower.{cc,hh} -- sets wifi txpower anno on a packet
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include "settxpower.hh"
CLICK_DECLS

SetTXPower::SetTXPower()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SetTXPower::~SetTXPower()
{
  MOD_DEC_USE_COUNT;
}

SetTXPower *
SetTXPower::clone() const
{
  return new SetTXPower;
}

int
SetTXPower::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _auto = false;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  "POWER", cpUnsigned, "power", &_power, 
		  "AUTO", cpBool, "auto power scaling", &_auto,
		  0) < 0) {
    return -1;
  }

  if (_power < 0 || _power > 200) {
    return errh->error("power must be 0,1,2,5, or 11");
  }

  return 0;
}

Packet *
SetTXPower::simple_action(Packet *p_in)
{
  
  SET_WIFI_TX_POWER_ANNO(p_in, _power);  
  return p_in;
}
String
SetTXPower::power_read_handler(Element *e, void *)
{
  SetTXPower *foo = (SetTXPower *)e;
  return String(foo->_power) + "\n";
}


String
SetTXPower::auto_read_handler(Element *e, void *)
{
  SetTXPower *foo = (SetTXPower *)e;
  return String(foo->_auto) + "\n";
}

void
SetTXPower::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("power", power_read_handler, 0);
  add_read_handler("auto", auto_read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXPower)

