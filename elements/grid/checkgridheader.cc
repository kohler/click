/*
 * checkgridheader.{cc,hh} -- element checks Grid header for correctness
 * (checksums, lengths)
 * Douglas S. J. De Couto
 * from checkipheader.{cc,hh} by Robert Morris
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
#include "checkgridheader.hh"
#include <click/glue.hh>
#include "grid.hh"
#include <clicknet/ether.h>
#include <clicknet/ip.h>
CLICK_DECLS

CheckGridHeader::CheckGridHeader()
  : _drops(0)
{
}

CheckGridHeader::~CheckGridHeader()
{
}

void
CheckGridHeader::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("CheckGridHeader %s: Grid checksum failed", name().c_str());
  _drops++;

  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

Packet *
CheckGridHeader::simple_action(Packet *p)
{
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));

  if(p->length() < sizeof(click_ether) + sizeof(grid_hdr)) {
#if 1
    click_chatter("%s: packet truncated", name().c_str());
#endif
    goto bad;
  }

  unsigned int hlen, tlen;
  hlen = gh->hdr_len;
  tlen = ntohs(gh->total_len);

  /* grid header size keeps changing
  if(hlen < sizeof(grid_hdr))
    goto bad;
  */

  if (ntohl(gh->version) != grid_hdr::GRID_VERSION) {
     click_chatter ("%s: unknown grid version %x", name().c_str(), ntohl(gh->version));
     p->kill();
     return 0;
  }

  if (tlen + sizeof(click_ether) > p->length()) {
    /* can only check inequality, as short packets are padded to a
       minimum frame size for wavelan and ethernet */
#if 1
    click_chatter("%s: bad packet size, wanted %d, only got %d", name().c_str(),
		  tlen + sizeof(click_ether), p->length());
#endif
    goto bad;
  }

  if (click_in_cksum((unsigned char *) gh, tlen) != 0) {
#if 1
    click_chatter("%s: bad Grid checksum", name().c_str());
    click_chatter("%s: length: %d, cksum: 0x%.4x",
		  name().c_str(), p->length(), (unsigned long) ntohs(gh->cksum));
#endif
    goto bad;
  }
  return(p);

 bad:
  drop_it(p);
  return 0;
}

static String
CheckGridHeader_read_drops(Element *xf, void *)
{
  CheckGridHeader *f = (CheckGridHeader *)xf;
  return String(f->drops()) + "\n";
}

void
CheckGridHeader::add_handlers()
{
  add_read_handler("drops", CheckGridHeader_read_drops, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckGridHeader)
