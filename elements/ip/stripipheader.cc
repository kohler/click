/*
 * stripipheader.{cc,hh} -- element removes IP header based on annotation
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "stripipheader.hh"
#include "click_ip.h"

StripIPHeader::StripIPHeader()
{
  add_input();
  add_output();
}

Packet *
StripIPHeader::simple_action(Packet *p)
{
  p->pull((int)p->ip_header_offset() + p->ip_header_length());
  return p;
}

EXPORT_ELEMENT(StripIPHeader)
