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
#include "ipaddress.hh"
#include "confparse.hh"

IP6Address::IP6Address()
{
  for (int i = 0; i < 4; i++)
    _addr.s6_addr32[i] = 0;
}

IP6Address::IP6Address(const unsigned char *data)
{
  const unsigned *udata = reinterpret_cast<const unsigned *>(data);
  for (int i = 0; i < 4; i++)
    _addr.s6_addr32[i] = udata[i];
}

IP6Address::IP6Address(const String &str)
{
  if (!cp_ip6_address(str, *this))
    for (int i = 0; i < 4; i++)
      _addr.s6_addr32[i] = 0;
}

bool
IP6Address::get_IP4Address(unsigned char ip4[4])
{
  if (_addr.s6_addr16[4] == 0 && _addr.s6_addr16[5] == 0xFFFF) {
    ip4[0] = _addr.s6_addr[12];
    ip4[1] = _addr.s6_addr[13];
    ip4[2] = _addr.s6_addr[14];
    ip4[3] = _addr.s6_addr[15];
    return true;
  }
 else 
   return false;
    
}

String
IP6Address::s() const
{
  char buf[48];
  
  // do some work to print the address well
  if (_addr.s6_addr32[0] == 0 && _addr.s6_addr32[1] == 0) {
    if (_addr.s6_addr32[2] == 0 && _addr.s6_addr32[3] == 0)
      return "::";		// empty address
    else if (_addr.s6_addr32[2] == 0) {
      sprintf(buf, "::%d.%d.%d.%d", _addr.s6_addr[12], _addr.s6_addr[13],
	      _addr.s6_addr[14], _addr.s6_addr[15]);
      return String(buf);
    } else if (_addr.s6_addr16[4] == 0 && _addr.s6_addr16[5] == 0xFFFF) {
      sprintf(buf, "::FFFF:%d.%d.%d.%d", _addr.s6_addr[12], _addr.s6_addr[13],
	      _addr.s6_addr[14], _addr.s6_addr[15]);
      return String(buf);
    }
  }

  char *s = buf;
  int word, n;
  for (word = 0; word < 8 && _addr.s6_addr16[word] != 0; word++) {
    sprintf(s, (word ? ":%X%n" : "%X%n"), ntohs(_addr.s6_addr16[word]), &n);
    s += n;
  }
  if (word == 0 || (word < 7 && _addr.s6_addr16[word+1] == 0)) {
    *s++ = ':';
    while (word < 8 && _addr.s6_addr16[word] == 0)
      word++;
    if (word == 8)
      *s++ = ':';
  }
  for (; word < 8; word++) {
    sprintf(s, ":%X%n", ntohs(_addr.s6_addr16[word]), &n);
    s += n;
  }
  *s++ = 0;
  return String(buf);
}

String
IP6Address::full_s() const
{
  char buf[48];
  sprintf(buf, "%X:%X:%X:%X:%X:%X:%X:%X",
	  ntohs(_addr.s6_addr16[0]), ntohs(_addr.s6_addr16[1]),
	  ntohs(_addr.s6_addr16[2]), ntohs(_addr.s6_addr16[3]),
	  ntohs(_addr.s6_addr16[4]), ntohs(_addr.s6_addr16[5]),
	  ntohs(_addr.s6_addr16[6]), ntohs(_addr.s6_addr16[7]));
  return String(buf);
}
