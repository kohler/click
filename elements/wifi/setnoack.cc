/*
 * setnoack.{cc,hh} -- sets wifi noack annotation on a packet
 * Lars Bro
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include "setnoack.hh"
CLICK_DECLS

SetNoAck::SetNoAck()
{
}

SetNoAck::~SetNoAck()
{
}

Packet *
SetNoAck::simple_action(Packet *p_in)
{
  uint8_t *dst_ptr = (uint8_t *) p_in->data();

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
  ceh->flags |= WIFI_EXTRA_TX_NOACK;

  return p_in;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetNoAck)

