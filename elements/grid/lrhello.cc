/*
 * lrhello.{cc,hh} -- Grid local routing protocol neighbor table broadcast element
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
#include "lrhello.hh"
#include <clicknet/ether.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include "grid.hh"
CLICK_DECLS

SendGridLRHello::SendGridLRHello()
  : _timer(this), _nbr(0), _hops(1)
{
}

SendGridLRHello::~SendGridLRHello()
{
}

int
SendGridLRHello::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int res = Args(conf, this, errh)
	.read_mp("PERIOD", _period)
	.read_mp("JITTER", _jitter)
	.read_mp("ETH", _from_eth)
	.read_mp("IP", _from_ip)
	.read_mp("UPDATEROUTES", reinterpret_cast<Element *&>(_nbr))
	.read_p("MAXHOPS", _hops)
	.complete();
  if (res < 0)
    return res;

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
  _timer.schedule_after_msec(_period); // Send periodically

  if(_nbr && _nbr->cast("UpdateLocalGridRoutes") == 0){
    errh->warning("%s: UpdateLocalGridRoutes argument %s has the wrong type",
                  name().c_str(),
                  _nbr->name().c_str());
    _nbr = 0;
  } else if (_nbr == 0) {
    errh->warning("%s: no UpdateLocalGridRoutes element given",
                  name().c_str());
  }

  return 0;
}

void
SendGridLRHello::run_timer(Timer *)
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

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SendGridLRHello)
