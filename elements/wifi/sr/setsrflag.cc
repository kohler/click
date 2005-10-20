/*
 * setsrflag.{cc,hh} -- print sr packets, for debugging.
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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
#include "setsrflag.hh"

CLICK_DECLS

SetSRFlag::SetSRFlag()
{
  _flags = 0;
}

SetSRFlag::~SetSRFlag()
{
}

int
SetSRFlag::configure(Vector<String> &conf, ErrorHandler* errh)
{
  uint16_t flags = 0;

  for (int i = 0; i < conf.size(); i++) {
    String text = conf[i];
    String word = cp_pop_spacevec(text);
    if (word == "FLAG_ERROR") {
      flags |= FLAG_ERROR;
    } else if (word == "FLAG_UPDATE") {
      flags |= FLAG_UPDATE;
    } else {
      return errh->error("unknown flag '%#s'", word.c_str());
    }
  }
  _flags = flags;
  return 0;
}

Packet *
SetSRFlag::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  pk->set_flag(_flags);
  return p;
}


String
SetSRFlag::static_print_flags(Element *e, void *)
{
  SetSRFlag *n = (SetSRFlag *) e;
  return n->print_flags();
}

String
SetSRFlag::print_flags()
{
  StringAccum sa;
  if (_flags & FLAG_ERROR) {
    sa << "FLAG_ERROR\n";
  }
  if (_flags & FLAG_UPDATE) {
    sa << "FLAG_UPDATE\n";
  }
  return sa.take_string();
}


void
SetSRFlag::add_handlers() 
{
  add_read_handler("print_flags", static_print_flags, 0);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(SetSRFlag)
