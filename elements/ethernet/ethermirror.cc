/*
 * ethermirror.{cc,hh} -- rewrites Ethernet packet a->b to b->a
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
#include "ethermirror.hh"
#include "click_ether.h"

Packet *
EtherMirror::simple_action(Packet *p)
{
  click_ether *ethh = (click_ether *)p->data();
  unsigned char tmpa[6];
  tmpa = ethh->ether_dhost;
  ethh->ether_shost = ethh->ether_dhost;
  ethh->ether_dhost = tmpa;
  return p;
}

EXPORT_ELEMENT(EtherMirror)
