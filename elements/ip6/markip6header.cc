/*
 * markip6header.{cc,hh} -- element sets IP6 Header annotation
 * Eddie Kohler, Peilei Fan
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
#include "markip6header.hh"
#include <click/confparse.hh>
#include <click/click_ip6.h>

MarkIP6Header::MarkIP6Header()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

MarkIP6Header::~MarkIP6Header()
{
  MOD_DEC_USE_COUNT;
}

MarkIP6Header *
MarkIP6Header::clone() const
{
  return new MarkIP6Header();
}

int
MarkIP6Header::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _offset = 0;
  return cp_va_parse(conf, this, errh,
		     cpOptional,
		     cpUnsigned, "offset to IP6 header", &_offset,
		     0);
}

Packet *
MarkIP6Header::simple_action(Packet *p)
{
  const click_ip6 *ip6 = reinterpret_cast<const click_ip6 *>(p->data() + _offset);
  p->set_ip6_header(ip6, 10 << 2);
  return p;
}

EXPORT_ELEMENT(MarkIP6Header)
