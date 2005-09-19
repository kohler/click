/*
 * scramble.{cc,hh} -- element scrambles packet data (for BIM-4xx-RS232 radios)
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
#include "scramble.hh"
CLICK_DECLS

Scramble::Scramble()
{
}

Scramble::~Scramble()
{
}

static unsigned char pattern[] = {
#if 1
0xa5, 0x0f, 0xe1, 0xe1, 0xe1, 0xc3, 0xe1, 0xc3,
0x3c, 0xc3, 0xd2, 0x1e, 0x87, 0x4b, 0x87, 0xa5,
0xe1, 0x4b, 0xe1, 0x0f, 0x3c, 0x87, 0x87, 0x1e,
0x2d, 0xa5, 0xb4, 0x69, 0x78, 0x1e, 0xc3, 0x1e,
0x2d, 0xb4, 0xf0, 0x1e, 0x78, 0xe1, 0xd2, 0xa5,
0xb4, 0xa5, 0xb4, 0x3c, 0xe1, 0x4b, 0xe1, 0xc3,
0x96, 0xd2, 0xc3, 0xc3, 0x5a, 0x5a, 0xe1, 0x78,
0xf0, 0x96, 0xe1, 0x69, 0xb4
#else
  0x2b, 0x1e, 0xc3, 0x43, 0xe2, 0xe7, 0xa2, 0xc7,
  0xf9, 0xe6, 0xc5, 0xbd, 0x0f, 0xf7, 0x4f, 0x0a,
  0x03, 0x16, 0x42, 0x3f, 0x18, 0x4f, 0x6f, 0x1c,
  0x7b, 0x2a, 0xc9, 0x93, 0x11, 0xbc, 0x26
#endif
};

Packet *
Scramble::simple_action(Packet *p_in)
{
  int i, len, j;

  WritablePacket *p = p_in->uniqueify();
  len = p->length();
  j = 0;
  for(i = 0; i < len; i++){
    p->data()[i] ^= pattern[j];
    j++;
    if(j >= (int)sizeof(pattern))
      j = 0;
  }

  return(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Scramble)
