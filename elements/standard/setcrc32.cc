/*
 * setcrc32.{cc,hh} -- element sets CRC 32 checksum
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

#include <click/config.h>
#include "setcrc32.hh"
extern "C" {
#include <click/crc32.h>
}

SetCRC32::SetCRC32()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SetCRC32::~SetCRC32()
{
  MOD_DEC_USE_COUNT;
}

Packet *
SetCRC32::simple_action(Packet *p)
{
  int len = p->length();
  unsigned int crc = 0xffffffff;
  crc = update_crc(crc, (char *) p->data(), len);

  WritablePacket *q = p->put(4);
  memcpy(q->data() + len, &crc, 4);
  
  return(q);
}

EXPORT_ELEMENT(SetCRC32)
ELEMENT_MT_SAFE(SetCRC32)
