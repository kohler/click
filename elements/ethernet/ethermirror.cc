/*
 * ethermirror.{cc,hh} -- rewrites Ethernet packet a->b to b->a
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
