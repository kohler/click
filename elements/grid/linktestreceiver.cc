/*
 * linktestreceiver.{cc,hh} -- receive and print link test packets
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2002 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include "linktestreceiver.hh"
#include "linktester.hh"
#include "airoinfo.hh"

CLICK_DECLS

LinkTestReceiver::LinkTestReceiver() : _ai(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

LinkTestReceiver::~LinkTestReceiver()
{
  MOD_DEC_USE_COUNT;
}

int
LinkTestReceiver::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpOptional,
			cpElement, "AiroInfo element", &_ai,
			cpEnd);
  if (res < 0)
    return res;
  return 0;
}

int
LinkTestReceiver::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
LinkTestReceiver::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  if (htons(eh->ether_type) != LinkTester::ETHERTYPE)
    return p;
  EtherAddress src(eh->ether_shost);
  EtherAddress dst(eh->ether_dhost);

  bool res = false;
  int dbm, quality;
  if (_ai) 
    res = _ai->get_signal_info(src, dbm, quality);
  if (!_ai || !res)
    dbm = quality = -1;

  res = false;
  int avg_over_sec, avg_over_minute, max_over_minute;
  if (_ai)
    res = _ai->get_noise(avg_over_sec, avg_over_minute, max_over_minute);
  if (!_ai || !res)
    avg_over_sec = avg_over_minute = max_over_minute = -1;

  LinkTester::payload_t *payload = (LinkTester::payload_t *) (eh + 1);
  
  struct timeval &tv = p->timestamp_anno();
  click_chatter("%u.%06u,%u.%06u,%s,%s,%hu,%d,%u,%u\n",
		tv.tv_sec, tv.tv_usec,
		ntohl(payload->tx_sec), ntohl(payload->tx_usec),
		src.s().cc(), dst.s().cc(), ntohs(payload->size),
		payload->before ? 1 : 0, ntohl(payload->iteration), 
		ntohl(payload->seq_no));
		
  return p;
}



ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LinkTestReceiver)
