/*
 * stationtable.{cc,hh} -- track stations for an ap
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
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "stationtable.hh"
CLICK_DECLS

StationTable::StationTable()
{

}

StationTable::~StationTable()
{
}

void
StationTable::take_state(Element *e, ErrorHandler *)
{
  StationTable *q = (StationTable *)e->cast("StationTable");
  if (!q) return;

  _table = q->_table;
}


enum {H_DEBUG};

static String
StationTable_read_param(Element *e, void *thunk)
{
  StationTable *td = (StationTable *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}
static int
StationTable_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  StationTable *f = (StationTable *)e;
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
StationTable::add_handlers()
{
  add_read_handler("debug", StationTable_read_param, H_DEBUG);

  add_write_handler("debug", StationTable_write_param, H_DEBUG);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(StationTable)

