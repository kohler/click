/*
 * checkipheader2.{cc,hh} -- element checks IP header for correctness
 * (checksums, lengths, source addresses)
 * Robert Morris
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
#include "checkipheader2.hh"
#include <clicknet/ip.h>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

CheckIPHeader2::CheckIPHeader2()
{
  // other stuff belongs to CheckIPHeader
  _checksum = false;
}

CheckIPHeader2::~CheckIPHeader2()
{
  // other stuff belongs to CheckIPHeader
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(CheckIPHeader)
EXPORT_ELEMENT(CheckIPHeader2)
