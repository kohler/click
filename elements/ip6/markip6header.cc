/*
 * markip6header.{cc,hh} -- element sets IP6 Header annotation
 * Eddie Kohler
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
#include "markip6header.hh"
#include "confparse.hh"
#include "click_ip6.h"

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
