/*
 * markipheader.{cc,hh} -- element sets IP Header annotation
 * Eddie Kohler
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
#include "markipheader.hh"
#include <click/confparse.hh>
#include <clicknet/ip.h>

MarkIPHeader::MarkIPHeader()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

MarkIPHeader::~MarkIPHeader()
{
  MOD_DEC_USE_COUNT;
}

MarkIPHeader *
MarkIPHeader::clone() const
{
  return new MarkIPHeader();
}

int
MarkIPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
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
ELEMENT_MT_SAFE(MarkIPHeader)
