/*
 * hello.{cc,hh} -- Grid HELLO broadcast element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/config.h>
#include <click/package.hh>
#include "hello.hh"
#include <click/click_ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/userlevel/tobpf.hh"
#include <click/router.hh>
#include "grid.hh"

SendGridHello::SendGridHello()
  : Element(0, 1), _timer(this)
{
  MOD_INC_USE_COUNT;
}

SendGridHello::~SendGridHello()
{
  MOD_DEC_USE_COUNT;
}

SendGridHello *
SendGridHello::clone() const
{
  return new SendGridHello;
}

int
SendGridHello::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "period (msec)", &_period,
			cpInteger, "jitter (msec)", &_jitter,
			cpEthernetAddress, "source Ethernet address", &_from_eth,
			cpIPAddress, "source IP address", &_from_ip,
			0);
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
  _timer.schedule_after_ms(_period); // Send Grid HELLO periodically

  return 0;
}

void
SendGridHello::run_scheduled()
{
  output(0).push(make_hello());

  // XXX this random stuff is not right i think... wouldn't it be nice
  // if click had a phat RNG like ns?
  int r2 = random();
  double r = (double) (r2 >> 1);
  int  jitter = (int) (((double) _jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  _timer.schedule_after_ms(_period + (int) jitter);
}

Packet *
SendGridHello::make_hello()
{
  int psz = sizeof(click_ether) + sizeof(grid_hdr);

  WritablePacket *p = Packet::make(psz);
  memset(p->data(), 0, p->length());

  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _from_eth.data(), 6);

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = htons(sizeof(grid_hdr));
  gh->type = grid_hdr::GRID_HELLO;
  gh->ip = _from_ip;

  return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SendGridHello)
