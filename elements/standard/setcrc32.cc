/*
 * setcrc32.{cc,hh} -- element sets CRC 32 checksum
 * Robert Morris
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
#include "setcrc32.hh"

extern "C" {
#include "crc32.h"
}

SetCRC32::SetCRC32()
{
  add_input();
  add_output();
}

SetCRC32::~SetCRC32()
{
}

Packet *
SetCRC32::simple_action(Packet *p)
{
  int len = p->length();
  unsigned int crc = 0xffffffff;
  crc = update_crc(crc, (char *) p->data(), len);

  Packet *q = p->put(4);
  memcpy(q->data() + len, &crc, 4);
  
  return(q);
}

EXPORT_ELEMENT(SetCRC32)
