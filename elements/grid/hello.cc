/*
 * hello.{cc,hh} -- Grid HELLO broadcast element
 * Douglas S. J. De Couto
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
#include "hello.hh"
#include <clicknet/ether.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include "grid.hh"
CLICK_DECLS

SendGridHello::SendGridHello()
  : _timer(this)
{
}

SendGridHello::~SendGridHello()
{
}

int
SendGridHello::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = Args(conf, this, errh)
      .read_mp("PERIOD", _period)
      .read_mp("JITTER", _jitter)
      .read_mp("ETH", _from_eth)
      .read_mp("IP", _from_ip)
      .complete();
  if (res < 0)
    return res;

  if (_period <= 0)
    return errh->error("period must be greater than 0");
  if (_jitter < 0)
    return errh->error("period must be positive");
  if (_jitter > _period)
    return errh->error("jitter is bigger than period");
  return res;
}

int
SendGridHello::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_msec(_period); // Send Grid HELLO periodically

  return 0;
}

void
SendGridHello::run_timer(Timer *)
{
  output(0).push(make_hello());

  // XXX this random stuff is not right i think... wouldn't it be nice
  // if click had a phat RNG like ns?
  uint32_t r2 = click_random();
  double r = (double) (r2 >> 1);
  int  jitter = (int) (((double) _jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  _timer.schedule_after_msec(_period + (int) jitter);
}

Packet *
SendGridHello::make_hello()
{
  int psz = sizeof(click_ether) + sizeof(grid_hdr);

  WritablePacket *p = Packet::make(psz + 2); // for alignment
  if (p == 0) {
    click_chatter("in %s: cannot make packet!", name().c_str());
    assert(0);
  }
  ASSERT_4ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  p->set_timestamp_anno(Timestamp::now());

  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _from_eth.data(), 6);

  grid_hdr *gh = (grid_hdr *) (eh + 1);
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = htons(sizeof(grid_hdr));
  gh->type = grid_hdr::GRID_HELLO;
  gh->ip = _from_ip;

  return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SendGridHello)
