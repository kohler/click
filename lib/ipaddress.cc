// -*- c-basic-offset: 2; related-file-name: "../include/click/ipaddress.hh" -*-
/*
 * ipaddress.{cc,hh} -- an IP address class. Useful for its hashcode()
 * method
 * Robert Morris / John Jannotti / Eddie Kohler
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

#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

IPAddress::IPAddress(const unsigned char *data)
{
#ifdef HAVE_INDIFFERENT_ALIGNMENT
  _addr = *(reinterpret_cast<const unsigned *>(data));
#else
  memcpy(&_addr, data, 4);
#endif
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
  uint32_t umask = 0;
  if (prefix > 0)
    umask = 0xFFFFFFFFU << (32 - prefix);
  return IPAddress(htonl(umask));
}

int
IPAddress::mask_to_prefix_len() const
{
  uint32_t host_addr = ntohl(_addr);
  uint32_t umask = 0xFFFFFFFFU;
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
  int prefix_len = mask_to_prefix_len();
  if (prefix_len >= 0)
    return String(prefix_len);
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
  amt = sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
  sa.append(buf, amt);
  return sa;
}
