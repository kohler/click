/*
 * ipaddress.{cc,hh} -- an IP address class. Useful for its hashcode()
 * method
 * Robert Morris / John Jannotti / Eddie Kohler
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipaddress.hh"
#include "confparse.hh"
#if CLICK_LINUXMODULE
extern "C" {
# include <linux/kernel.h>
}
#else
# include <stdio.h>
#endif

IPAddress::IPAddress(const unsigned char *data)
{
  _addr = *(reinterpret_cast<const unsigned *>(data));
}

IPAddress::IPAddress(const String &str)
{
  if (!cp_ip_address(str, this))
    _addr = 0;
}

String
IPAddress::s() const
{
  const unsigned char *p = data();
  char buf[20];
  sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
  return String(buf);
}
