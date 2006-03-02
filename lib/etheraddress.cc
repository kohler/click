// -*- c-basic-offset: 2; related-file-name: "../include/click/etheraddress.hh" -*-
/*
 * etheraddress.{cc,hh} -- an Ethernet address class. Useful for its
 * hashcode() method
 * Robert Morris / John Jannotti, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2006 Regents of the University of California
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
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
CLICK_DECLS

/** @class EtherAddress
    @brief An Ethernet address.

    The EtherAddress type represents an Ethernet address. It supports equality
    operations and provides methods for unparsing addresses into ASCII
    colon-separated hex form. */

/** @brief Unparses this address into a colon-separated hex String.

    Examples include "00:00:00:00:00:00" and "00:05:4E:50:3C:1A". */
String
EtherAddress::unparse() const
{
  char buf[24];
  const unsigned char *p = this->data();
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
	  p[0], p[1], p[2], p[3], p[4], p[5]);
  return String(buf, 17);
}

StringAccum &
operator<<(StringAccum &sa, const EtherAddress &ea)
{
  char buf[24];
  const unsigned char *p = ea.data();
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
	  p[0], p[1], p[2], p[3], p[4], p[5]);
  sa.append(buf, 17);
  return sa;
}

CLICK_ENDDECLS
