/*
 * markipheader.{cc,hh} -- element sets IP Header annotation
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
#include "markipheader.hh"
#include "confparse.hh"
#include "click_ip.h"

MarkIPHeader::MarkIPHeader()
{
  add_input();
  add_output();
}

MarkIPHeader *
MarkIPHeader::clone() const
{
  return new MarkIPHeader();
}

int
MarkIPHeader::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _offset = 0;
  return cp_va_parse(conf, this, errh,
		     cpOptional,
		     cpUnsigned, "offset to IP header", &_offset,
		     0);
}

Packet *
MarkIPHeader::simple_action(Packet *p)
{
  const click_ip *ip = reinterpret_cast<const click_ip *>(p->data() + _offset);
  p->set_ip_header(ip, ip->ip_hl << 2);
  return p;
}

EXPORT_ELEMENT(MarkIPHeader)
