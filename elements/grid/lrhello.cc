/*
 * lrhello.{cc,hh} -- Grid local routing protocol neighbor table broadcast element
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

#include <click/config.h>
#include "lrhello.hh"
#include <click/click_ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/userlevel/tobpf.hh"
#include <click/router.hh>
#include "grid.hh"

SendGridLRHello::SendGridLRHello()
  : Element(0, 1), _timer(this), _nbr(0), _hops(1)
{
  MOD_INC_USE_COUNT;
}

SendGridLRHello::~SendGridLRHello()
{
  MOD_DEC_USE_COUNT;
}

SendGridLRHello *
SendGridLRHello::clone() const
{
  return new SendGridLRHello;
}

int
SendGridLRHello::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "period (msec)", &_period,
			cpInteger, "jitter (msec)", &_jitter,
			cpEthernetAddress, "source Ethernet address", &_from_eth,
			cpIPAddress, "source IP address", &_from_ip,
                        cpElement, "UpdateGridRoutes element", &_nbr,
			cpOptional,
			cpInteger, "max nbr hops", &_hops,
			0);
  if (_period <= 0)
    return errh->error("period must be greater than 0");
  if (_jitter < 0)
    return errh->error("period must be positive");
  if (_jitter > _period)
    return errh->error("jitter is bigger than period");
  if (_hops < 0)
    return errh->error("max hops must be greater than 0");
  return res;
}

int
SendGridLRHello::initialize(ErrorHandler *errh)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(_period); // Send periodically

  if(_nbr && _nbr->cast("UpdateLocalGridRoutes") == 0){
    errh->warning("%s: UpdateLocalGridRoutes argument %s has the wrong type",
                  id().cc(),
                  _nbr->id().cc());
    _nbr = 0;
  } else if (_nbr == 0) {
    errh->warning("%s: no UpdateLocalGridRoutes element given",
                  id().cc());
  }

  return 0;
}

void
SendGridLRHello::run_scheduled()
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
SendGridLRHello::make_hello()
{
  int psz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_hello);
  int num_nbrs = 0;
  Vector<grid_nbr_entry> nbrs;
  if (_nbr) {
    _nbr->get_rtes(&nbrs);
    // don't count nbrs that are too many hops away
    for (int i = 0; i < nbrs.size(); i++) 
      if (nbrs[i].num_hops <= _hops)
	num_nbrs++;
    psz += sizeof(grid_nbr_entry) * num_nbrs;
  }

  WritablePacket *p = Packet::make(psz + 2);
  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  struct timeval tv;
  int res = gettimeofday(&tv, 0);
  if (res == 0) 
    p->set_timestamp_anno(tv);

  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _from_eth.data(), 6);

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = psz - sizeof(click_ether);
  gh->total_len = htons(gh->total_len);
  gh->type = grid_hdr::GRID_LR_HELLO;
  gh->ip = gh->tx_ip = _from_ip;
  grid_hello *hlo = (grid_hello *) (p->data() + sizeof(click_ether) + sizeof(grid_hdr));
  assert(num_nbrs <= 255);
  hlo->num_nbrs = (unsigned char) num_nbrs;
click_chatter("num_nbrs = %d , _hops = %d, nbrs.size() = %d",
              num_nbrs, _hops, nbrs.size());
  hlo->nbr_entry_sz = sizeof(grid_nbr_entry);

  grid_nbr_entry *curr = (grid_nbr_entry *) (p->data() + sizeof(click_ether) +
					     sizeof(grid_hdr) + sizeof(grid_hello));
  for (int i = 0; i < nbrs.size(); i++) {
    // only include nbrs that are not too many hops away
    if (nbrs[i].num_hops <= _hops) {
      memcpy(curr, &nbrs[i], sizeof(grid_nbr_entry));
      curr++;
    }
  }

  return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SendGridLRHello)

