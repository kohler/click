/*
 * setannobyte.{cc,hh} -- element sets packets' user annotation
 * John Bicket
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "setannobyte.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
CLICK_DECLS

SetAnnoByte::SetAnnoByte()
  : Element(1, 1), _offset(0), _value(0)
{
  MOD_INC_USE_COUNT;
}

SetAnnoByte::~SetAnnoByte()
{
  MOD_DEC_USE_COUNT;
}

int
SetAnnoByte::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpUnsigned, "offset", &_offset,
			cpByte, "value", &_value,
			cpEnd);
  if (res < 0) 
    return res;

  if (_offset >= Packet::USER_ANNO_SIZE) 
    return errh->error("offset value is too large, max valid offset is %u", Packet::USER_ANNO_SIZE - 1);

  return res;
}

Packet *
SetAnnoByte::simple_action(Packet *p)
{
  (p)->set_user_anno_c(_offset, _value);
  return p;
}

String
SetAnnoByte::offset_read_handler(Element *e, void *)
{
  SetAnnoByte *anno = (SetAnnoByte *)e;
  StringAccum sa;
  sa << anno->_offset << "\n";
  return sa.take_string();
}


String
SetAnnoByte::value_read_handler(Element *e, void *)
{
  SetAnnoByte *anno = (SetAnnoByte *)e;
  StringAccum sa;
  sa << (int)anno->_value << "\n";
  return sa.take_string();
}

void
SetAnnoByte::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("offset", offset_read_handler, (void *)0);
  add_read_handler("value", value_read_handler, (void *)0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetAnnoByte)
ELEMENT_MT_SAFE(SetAnnoByte)
