/*
 * setwifiextraflag.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "setwifiextraflag.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

SetWifiExtraFlag::SetWifiExtraFlag()
{
}

SetWifiExtraFlag::~SetWifiExtraFlag()
{
}

int
SetWifiExtraFlag::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "flags", &_flag,
		  cpKeywords, 
		  cpEnd) < 0) {
    return -1;
  }
  return 0;
}

Packet *
SetWifiExtraFlag::simple_action(Packet *p)
{

  if (p) {
    struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();
    ceh->magic = WIFI_EXTRA_MAGIC;
    ceh->flags |= _flag;
  }
  return p;
}

enum {H_FLAG};
static String
SetWifiExtraFlag_read_param(Element *e, void *thunk)
{
  SetWifiExtraFlag *td = (SetWifiExtraFlag *)e;
  switch ((uintptr_t) thunk) {
  case H_FLAG:
    return String(td->_flag) + "\n";
  default:
    return String();
  }

}

static int 
SetWifiExtraFlag_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SetWifiExtraFlag *f = (SetWifiExtraFlag *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_FLAG: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("stepup parameter must be unsigned");
    f->_flag = m;
    break;
  }
  }
    return 0;

}
void
SetWifiExtraFlag::add_handlers()
{
  add_read_handler("flag", SetWifiExtraFlag_read_param, (void *) H_FLAG);
  add_write_handler("flag", SetWifiExtraFlag_write_param, (void *) H_FLAG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetWifiExtraFlag)

