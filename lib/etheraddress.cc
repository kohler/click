/*
 * etheraddress.{cc,hh} -- an Ethernet address class. Useful for its
 * hashcode() method
 * Robert Morris / John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>

#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#if CLICK_LINUXMODULE
extern "C" {
# include <linux/kernel.h>
}
#else
# include <stdio.h>
#endif

EtherAddress::EtherAddress(unsigned char *addr)
{
  memcpy(data(), addr, 6);
}

String
EtherAddress::s() const
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
