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

#include <click/config.h>

#include <click/ipaddress.hh>
#include <click/confparse.hh>
#if CLICK_LINUXMODULE
extern "C" {
# include <linux/kernel.h>
}
#else
# include <stdio.h>
#endif
#include <click/straccum.hh>

IPAddress::IPAddress(const unsigned char *data)
{
  _addr = *(reinterpret_cast<const unsigned *>(data));
}

IPAddress::IPAddress(const String &str)
{
  if (!cp_ip_address(str, this))
    _addr = 0;
}

IPAddress
IPAddress::make_prefix(int prefix)
{
  assert(prefix >= 0 && prefix <= 32);
  u_int32_t umask = 0;
  if (prefix > 0)
    umask = 0xFFFFFFFFU << (32 - prefix);
  return IPAddress(htonl(umask));
}

int
IPAddress::mask_to_prefix_bits() const
{
  u_int32_t host_addr = ntohl(_addr);
  u_int32_t umask = 0xFFFFFFFFU;
  for (int i = 32; i >= 0; i--, umask <<= 1)
    if (host_addr == umask)
      return i;
  return -1;
}

String
IPAddress::unparse() const
{
  const unsigned char *p = data();
  char buf[20];
  sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
  return String(buf);
}

String
IPAddress::unparse_mask() const
{
  int prefix_bits = mask_to_prefix_bits();
  if (prefix_bits >= 0)
    return String(prefix_bits);
  else
    return unparse();
}

String
IPAddress::unparse_with_mask(IPAddress mask) const
{
  return unparse() + "/" + mask.unparse_mask();
}

StringAccum &
operator<<(StringAccum &sa, IPAddress ipa)
{
  const unsigned char *p = ipa.data();
  char buf[20];
  int amt;
  sprintf(buf, "%d.%d.%d.%d%n", p[0], p[1], p[2], p[3], &amt);
  sa.append(buf, amt);
  return sa;
}
