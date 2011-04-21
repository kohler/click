/*
 * setgridchecksum.{cc,hh} -- element sets Grid header checksum
 * Douglas S. J. De Couto
 * adapted from setipchecksum.{cc,hh} by Robert Morris
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
#include <click/args.hh>
#include "setgridchecksum.hh"
#include <click/glue.hh>
#include "grid.hh"
#include <clicknet/ether.h>
#include <clicknet/ip.h>
CLICK_DECLS

SetGridChecksum::SetGridChecksum()
{
}

SetGridChecksum::~SetGridChecksum()
{
}

Packet *
SetGridChecksum::simple_action(Packet *xp)
{
  WritablePacket *p = xp->uniqueify();
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  unsigned plen = p->length();
  unsigned int tlen = ntohs(gh->total_len);

  if (!gh || plen < sizeof(grid_hdr) + sizeof(click_ether))
    goto bad;

  if (/* hlen < sizeof(grid_hdr) || */ // grid_hdr size keeps changing...
      tlen > plen - sizeof(click_ether))
    goto bad;

  gh->version = htonl(grid_hdr::GRID_VERSION);

  gh->cksum = 0;
  gh->cksum = click_in_cksum((unsigned char *) gh, tlen);

  return p;

 bad:
  click_chatter("SetGridChecksum: bad lengths");
  p->kill();
  return(0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetGridChecksum)
