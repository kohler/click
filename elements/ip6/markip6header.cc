/*
 * markip6header.{cc,hh} -- element sets IP6 Header annotation
 * Eddie Kohler
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
#include "markip6header.hh"
#include <click/confparse.hh>
#include <click/click_ip6.h>

MarkIP6Header::MarkIP6Header()
{
  add_input();
  add_output();
}

MarkIP6Header *
MarkIP6Header::clone() const
{
  return new MarkIP6Header();
}

int
MarkIP6Header::configure(const Vector<String> &conf, ErrorHandler *errh)
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
