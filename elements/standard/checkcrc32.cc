/*
 * checkcrc32.{cc,hh} -- element checks CRC 32 checksum
 * Robert Morris
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
#include "checkcrc32.hh"

extern "C" {
#include "crc32.h"
}

CheckCRC32::CheckCRC32()
{
  add_input();
  add_output();
  _drops = 0;
}

CheckCRC32::~CheckCRC32()
{
}

Packet *
CheckCRC32::simple_action(Packet *p)
{
  Packet *q = 0;
  unsigned int crc;

  int len = p->length();
  if(len < 4)
    goto drop;

  crc = update_crc(0xffffffff, (char *) p->data(), len - 4);

  unsigned int pcrc;
  memcpy(&pcrc, p->data() + len - 4, 4);
  if(pcrc != crc)
    goto drop;

  q = p->take(4);
  
  return(q);

 drop:
  click_chatter("CRC32 failed, len %d",
              p->length());
  p->kill();
  _drops++;
  return(0);
}


EXPORT_ELEMENT(CheckCRC32)
