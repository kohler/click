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

  WritablePacket *q = p->put(4);
  memcpy(q->data() + len, &crc, 4);
  
  return(q);
}

EXPORT_ELEMENT(SetCRC32)
