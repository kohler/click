/*
 * checkcrc32.{cc,hh} -- element checks CRC 32 checksum
 * Robert Morris
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
#include <click/config.h>
#include <click/package.hh>
#include "checkcrc32.hh"

extern "C" {
#include <click/crc32.h>
}

CheckCRC32::CheckCRC32()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _drops = 0;
}

CheckCRC32::~CheckCRC32()
{
  MOD_DEC_USE_COUNT;
}

Packet *
CheckCRC32::simple_action(Packet *p)
{
  unsigned int crc;

  int len = p->length();
  if(len < 4)
    goto drop;

  crc = update_crc(0xffffffff, (char *) p->data(), len - 4);

  unsigned int pcrc;
  memcpy(&pcrc, p->data() + len - 4, 4);
  if(pcrc != crc)
    goto drop;

  p->take(4);
  return p;

 drop:
  click_chatter("CRC32 failed, len %d",
              p->length());
  p->kill();
  _drops++;
  return 0;
}


EXPORT_ELEMENT(CheckCRC32)
