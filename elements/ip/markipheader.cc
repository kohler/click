/*
 * markipheader.{cc,hh} -- element sets IP Header annotation
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
MarkIPHeader::configure(const String &conf, ErrorHandler *errh)
{
  _offset = 0;
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "offset to IP header", &_offset,
		     0);
}

Packet *
MarkIPHeader::simple_action(Packet *p)
{
  click_ip *ip = (click_ip *)(p->data() + _offset);
  p->set_ip_header(ip, ip->ip_hl << 2);
  return p;
}

EXPORT_ELEMENT(MarkIPHeader)
