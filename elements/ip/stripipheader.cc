/*
 * stripipheader.{cc,hh} -- element removes IP header based on annotation
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <click/config.h>
#include <click/package.hh>
#include "stripipheader.hh"
#include <click/click_ip.h>

StripIPHeader::StripIPHeader()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

StripIPHeader::~StripIPHeader()
{
  MOD_DEC_USE_COUNT;
}

Packet *
StripIPHeader::simple_action(Packet *p)
{
  p->pull((int)p->ip_header_offset() + p->ip_header_length());
  return p;
}

EXPORT_ELEMENT(StripIPHeader)
ELEMENT_MT_SAFE(StripIPHeader)
