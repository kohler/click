/*
 * ip6address.{cc,hh} -- an IP6 address class. Useful for its hashcode()
 * method
 * Robert Morris / John Jannotti / Peilei Fan
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ip6address.hh"
#include "confparse.hh"


IP6Address::IP6Address(const unsigned char *data)
{
  _addr = new ip6_addr(data);
}

IP6Address::IP6Address(const String &str)
{
#ifdef __KERNEL__
  printk("<1>IP6Address::IP6Address?\n");
#else
  if (!cp_ip6_address(str, (unsigned char *)&_addr))
    _addr = new ip6_addr();
#endif
}

String
IP6Address::s() const
{
  unsigned char *p = (unsigned char *)&(_addr.add);
  String s;
  char tmp[256];
   sprintf(tmp, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
  return String(tmp);
}

void
IP6Address::print(void)
{
  unsigned char *p = (unsigned char *)&(_addr.add);
#ifdef __KERNEL__
  printk("<1>IP6Address::print?\n");
#else
  printf("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
#endif
}



