/*
 * unstripipheader.{cc,hh} -- put IP header back based on annotation
 * Benjie Chen
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
#include <config.h>
#endif
#include "unstripipheader.hh"
#include "click_ip.h"

UnstripIPHeader::UnstripIPHeader()
{
  add_input();
  add_output();
}

Packet *
UnstripIPHeader::simple_action(Packet *p)
{
  int offset = (unsigned char*) p->ip_header() - p->data();
  if (offset < 0) 
    p->push(0-offset);
  return p;
}

EXPORT_ELEMENT(UnstripIPHeader)
