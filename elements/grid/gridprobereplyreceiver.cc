/*
 * gridprobereplyreceiver.{cc,hh} -- element that receives Grid probe reply packets
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "gridprobereplyreceiver.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/router.hh>
#include "grid.hh"
#include "timeutils.hh"
CLICK_DECLS

GridProbeReplyReceiver::GridProbeReplyReceiver()
{
}

int
GridProbeReplyReceiver::initialize(ErrorHandler *)
{
  return 0;
}

GridProbeReplyReceiver::~GridProbeReplyReceiver()
{
}


int
GridProbeReplyReceiver::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String chan;
  int res = Args(conf, this, errh)
      .read_mp("CHANNEL", chan)
      .complete();
  if (res < 0)
    return res;
  _repl_errh = router()->chatter_channel(chan);
  assert(_repl_errh);
  return res;
}


Packet *
GridProbeReplyReceiver::simple_action(Packet *p)
{
  click_ether *e = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (e + 1);
  grid_nbr_encap *nb = (grid_nbr_encap *) (gh + 1);
  grid_route_reply *rr = (grid_route_reply *) (nb + 1);

  if (gh->type != grid_hdr::GRID_ROUTE_REPLY) {
    click_chatter("GridProbeReplyReceiver %s: received unexpected Grid packet type %s; is the configuration wrong?",
		  name().c_str(), grid_hdr::type_string(gh->type).c_str());
    p->kill();
    return 0;
  }

  struct timeval tx_time = rr->probe_send_time;
  tx_time.tv_sec = ntohl(tx_time.tv_sec);
  tx_time.tv_usec = ntohl(tx_time.tv_usec);

  struct timeval rtt = p->timestamp_anno().timeval() - tx_time;

  char buf[200];
  snprintf(buf, sizeof(buf),
	   "dest=%s nonce=%u hop=%s hopcount=%u rtt=%ld.%06ld tx_time=%ld.%06ld act=%u d1=%u d2=%u",
	   IPAddress(rr->probe_dest).unparse().c_str(),
	   (unsigned int) ntohl(rr->nonce),
	   IPAddress(gh->ip).unparse().c_str(),
	   (unsigned int) rr->reply_hop,
	   (long) rtt.tv_sec, (long) rtt.tv_usec,
	   (long) tx_time.tv_sec,
	   (long) tx_time.tv_usec,
	   (unsigned int) ntohl(rr->route_action),
	   (unsigned int) ntohl(rr->data1),
	   (unsigned int) ntohl(rr->data2));

  _repl_errh->message(buf);
  p->kill();
  return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridProbeReplyReceiver)
