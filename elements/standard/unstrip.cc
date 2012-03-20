/*
 * unstrip.{cc,hh} -- element unstrips bytes from front of packet
 * Robert Morris, Eddie Kohler
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
#include "unstrip.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

Unstrip::Unstrip(unsigned nbytes)
  : _nbytes(nbytes)
{
}

int
Unstrip::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("LENGTH", _nbytes).complete();
}

Packet *
Unstrip::simple_action(Packet *p)
{
  return p->push(_nbytes);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Unstrip)
ELEMENT_MT_SAFE(Unstrip)
