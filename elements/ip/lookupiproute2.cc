/*
 * lookupiproute2.{cc,hh} -- element looks up next-hop address in
 * pokeable routing table. renamed to iplookupradix.
 *
 * Thomer M. Gil
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
#include "lookupiproute2.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

LookupIPRoute2::LookupIPRoute2()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

LookupIPRoute2::~LookupIPRoute2()
{
  MOD_DEC_USE_COUNT;
}

int
LookupIPRoute2::configure(Vector<String> &, ErrorHandler *errh)
{
  return errh->error("LookupIPRoute2 has been renamed as RadixIPLookup");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LookupIPRoute2)
