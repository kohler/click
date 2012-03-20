/*
 * checkcrc32.{cc,hh} -- element checks CRC 32 checksum
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
#include "checkcrc32.hh"
CLICK_DECLS

CheckCRC32::CheckCRC32()
{
  _drops = 0;
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

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckCRC32)
ELEMENT_MT_SAFE(CheckCRC32)
