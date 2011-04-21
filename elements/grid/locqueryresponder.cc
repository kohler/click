/*
 * locqueryresponder.{cc,hh} -- element that responds to Grid location queries
 * Douglas S. J. De Couto
 * based on arpresponder.{cc,hh} by Robert Morris
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
#include "locqueryresponder.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "grid.hh"
CLICK_DECLS

#define NOISY 1

LocQueryResponder::LocQueryResponder()
  : _expire_timer(expire_hook, this)
{
}

int
LocQueryResponder::initialize(ErrorHandler *)
{
  _expire_timer.initialize(this);
  _expire_timer.schedule_after_msec(EXPIRE_TIMEOUT_MS);
  _timeout_jiffies = (CLICK_HZ * EXPIRE_TIMEOUT_MS) / 1000;
  return 0;
}

LocQueryResponder::~LocQueryResponder()
{
}

int
LocQueryResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("ETH", _eth)
	.read_mp("IP", _ip)
	.complete();
}

void
LocQueryResponder::expire_hook(Timer *, void *thunk)
{
  LocQueryResponder *resp = (LocQueryResponder *)thunk;
  unsigned int jiff = click_jiffies();

  // flush old ``last query heard''
  typedef seq_map::iterator smi_t;
  Vector<IPAddress> old_seqs;
  for (smi_t i = resp->_query_seqs.begin(); i.live(); i++)
    if (jiff - i.value().last_jiffies > resp->_timeout_jiffies)
      old_seqs.push_back(i.key());

  for (int i = 0; i < old_seqs.size(); i++)
    resp->_query_seqs.remove(old_seqs[i]);

  resp->_expire_timer.schedule_after_msec(EXPIRE_TIMEOUT_MS);
}


Packet *
LocQueryResponder::simple_action(Packet *p)
{
  click_ether *e = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (e + 1);
  grid_loc_query *lq = (grid_loc_query *) (gh + 1);

  if (gh->type != grid_hdr::GRID_LOC_QUERY) {
    click_chatter("LocQueryResponder %s: received unexpected Grid packet type %s; is the configuration wrong?",
		  name().c_str(), grid_hdr::type_string(gh->type).c_str());
    p->kill();
    return 0;
  }

  if (lq->dst_ip != (unsigned int) _ip) {
    click_chatter("LocQueryResponder %s: received location query for someone else (%s); is the configuration wrong?",
		  name().c_str(), IPAddress(lq->dst_ip).unparse().c_str());
    p->kill();
    return 0;
  }

  // ignore queries that are old
  unsigned int seq_no = ntohl(lq->seq_no);
  seq_t *old_seq = _query_seqs.findp(gh->ip);
  if (old_seq && old_seq->seq_no >= seq_no) {
#if NOISY
    click_chatter("LocQueryResponder %s: ignoring old query from %s (%u) ", name().c_str(), IPAddress(gh->ip).unparse().c_str(), seq_no);
#endif
    p->kill();
    return 0;
  }
  _query_seqs.insert(gh->ip, seq_t(seq_no, click_jiffies()));

  // construct the response
  WritablePacket *q = Packet::make(sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap) + 2);
  if (q == 0) {
    click_chatter("in %s: cannot make packet!", name().c_str());
    assert(0);
  }
  ASSERT_4ALIGNED(q->data());
  q->pull(2);

  p->set_timestamp_anno(Timestamp::now());

  memset(q->data(), 0, q->length());
  e = (click_ether *) q->data();
  grid_hdr *ngh = (grid_hdr *) (e + 1);
  grid_nbr_encap *nb = (grid_nbr_encap *) (ngh + 1);

  e->ether_type = htons(ETHERTYPE_GRID);
  // leave ether src, dest for the forwarding elements to fill in

  ngh->hdr_len = sizeof(grid_hdr);
  ngh->type = grid_hdr::GRID_LOC_REPLY;
  ngh->ip = ngh->tx_ip = _ip;
  ngh->total_len = htons(q->length() - sizeof(click_ether));

  nb->dst_ip = gh->ip;
#ifndef SMALL_GRID_HEADERS
  nb->dst_loc = gh->loc;
  nb->dst_loc_err = gh->loc_err; // don't need to convert, already in network byte order
  nb->dst_loc_good = gh->loc_good;
#endif
  nb->hops_travelled = 0;

  p->kill();
  return(q);
}

EXPORT_ELEMENT(LocQueryResponder)
CLICK_ENDDECLS
