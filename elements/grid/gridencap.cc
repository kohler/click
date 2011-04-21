/*
 * gridencap.{cc,hh} -- element encapsulates packet in Grid data header
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <elements/grid/grid.hh>
#include <elements/grid/gridencap.hh>
#include <click/args.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/glue.hh>

CLICK_DECLS

GridEncap::GridEncap()
{
}

GridEncap::~GridEncap()
{
}

int
GridEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (Args(conf, this, errh)
      .read_mp("ETH", _eth)
      .read_mp("IP", _ip)
      .complete() < 0)
      return -1;

  memset(&_eh, 0, sizeof(_eh));
  memset(&_gh, 0, sizeof(_gh));
  memset(&_nb, 0, sizeof(_nb));

  _eh.ether_type = htons(ETHERTYPE_GRID);
  memcpy(_eh.ether_shost, _eth.data(), 6);

  _gh.version = htonl(grid_hdr::GRID_VERSION);
  _gh.hdr_len = sizeof(grid_hdr);
  _gh.type = grid_hdr::GRID_NBR_ENCAP;
  _gh.ip = _ip.addr();
  _gh.tx_ip = _ip.addr();
  // _gh.total_len is set when packet is actually processed
  // _gh.cksum should be calculated by SetGridChecksum element

  // _nb.dst_ip is set from the packet's dst_ip_anno
#ifndef SMALL_GRID_HEADERS
  _nb.dst_loc_good = false;
#endif
  _nb.hops_travelled = 0;

  return 0;
}

int
GridEncap::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
GridEncap::simple_action(Packet *p_in)
{
  int extra = sizeof(_eh) + sizeof(_gh) + sizeof(_nb);
  WritablePacket *p = p_in->push(extra);
  if (!p)
    return 0;

  _gh.total_len = htons(p->length() - sizeof(_eh));
  _nb.dst_ip = p->dst_ip_anno();

  memcpy(p->data(), &_eh, sizeof(_eh));
  memcpy(p->data() + sizeof(_eh), &_gh, sizeof(_gh));
  memcpy(p->data() + sizeof(_eh) + sizeof(_gh), &_nb, sizeof(_nb));

  return p;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(GridEncap)

