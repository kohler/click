/*
 * unstripipheader.{cc,hh} -- put IP header back based on annotation
 * Benjie Chen
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
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
