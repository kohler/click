/*
 * ipaddress.{cc,hh} -- an IP address class. Useful for its hashcode()
 * method
 * Robert Morris / John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
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
  if (!cp_ip_address(str, *this))
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
