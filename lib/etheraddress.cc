/*
 * etheraddress.{cc,hh} -- an Ethernet address class. Useful for its
 * hashcode() method
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
#include "etheraddress.hh"
#include "glue.hh"

EtherAddress::EtherAddress(unsigned char *addr)
{
  memcpy(data(), addr, 6);
}

bool
EtherAddress::is_group() {
  return ((char*)_data)[0] & 1;
}

String
EtherAddress::s() const {
  char buf[20];
  const unsigned char *p = this->data();

  sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
	  p[0], p[1], p[2], p[3], p[4], p[5]);

  return buf;
}
