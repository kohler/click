// -*- c-basic-offset: 2; related-file-name: "../include/click/etheraddress.hh" -*-
/*
 * etheraddress.{cc,hh} -- an Ethernet address class. Useful for its
 * hashcode() method
 * Robert Morris / John Jannotti
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
#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
CLICK_DECLS

EtherAddress::EtherAddress(const unsigned char *addr)
{
  memcpy(data(), addr, 6);
}

String
EtherAddress::unparse() const
{
  char buf[24];
  const unsigned char *p = this->data();
  sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
	  p[0], p[1], p[2], p[3], p[4], p[5]);
  return String(buf, 17);
}

StringAccum &
operator<<(StringAccum &sa, const EtherAddress &ea)
{
  char buf[24];
  const unsigned char *p = ea.data();
  sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
	  p[0], p[1], p[2], p[3], p[4], p[5]);
  sa.append(buf, 17);
  return sa;
}

CLICK_ENDDECLS
