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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include "settxrate.hh"
CLICK_DECLS

SetTXRate::SetTXRate()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SetTXRate::~SetTXRate()
{
  MOD_DEC_USE_COUNT;
}

SetTXRate *
SetTXRate::clone() const
{
  return new SetTXRate;
}

int
SetTXRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _auto = false;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  "RATE", cpUnsigned, "rate", &_rate, 
		  "AUTO", cpBool, "auto rate scaling", &_auto,
		  0) < 0) {
    return -1;
  }

  switch (_rate) {
  case 0: 
    /* fallthrough */
  case 1:
    /* fallthrough */
  case 2:
    /* fallthrough */
  case 5:
    /* fallthrough */
  case 11:
    break;
  default:
    return errh->error("rate must be 0,1,2,5, or 11");
  }

  return 0;
}

Packet *
SetTXRate::simple_action(Packet *p_in)
{
  if (_auto) {
    SET_WIFI_RATE_ANNO(p_in, 0);  
  } else {
    SET_WIFI_RATE_ANNO(p_in, _rate);  
  }
  return p_in;
}
String
SetTXRate::rate_read_handler(Element *e, void *)
{
  SetTXRate *foo = (SetTXRate *)e;
  return String(foo->_rate) + "\n";
}
String
SetTXRate::auto_read_handler(Element *e, void *)
{
  SetTXRate *foo = (SetTXRate *)e;
  return String(foo->_auto) + "\n";
}

void
SetTXRate::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("rate", rate_read_handler, 0);
  add_read_handler("auto", auto_read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXRate)

