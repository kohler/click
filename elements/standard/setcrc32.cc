/*
 * setcrc32.{cc,hh} -- element sets CRC 32 checksum
 * Robert Morris
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
#include <click/crc32.h>
#include "setcrc32.hh"
CLICK_DECLS

SetCRC32::SetCRC32()
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

CLICK_ENDDECLS
EXPORT_ELEMENT(SetCRC32)
ELEMENT_MT_SAFE(SetCRC32)
