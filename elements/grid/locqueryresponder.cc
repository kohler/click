/*
 * locqueryresponder.{cc,hh} -- element that responds to Grid location queries
 * Douglas S. J. De Couto
 * based on arpresponder.{cc,hh} by Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "locqueryresponder.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#include "grid.hh"

LocQueryResponder::LocQueryResponder()
{
  add_input();
  add_output();
}

LocQueryResponder::~LocQueryResponder()
{
}

LocQueryResponder *
LocQueryResponder::clone() const
{
  return new LocQueryResponder;
}
int
LocQueryResponder::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpEthernetAddress, "Ethernet address", &_eth,
		     cpIPAddress, "IP address", &_ip,
		     0);
}


Packet *
LocQueryResponder::simple_action(Packet *p)
{
  click_ether *e = (click_ether *) p->data();
  grid_hdr *gh = (grid_hdr *) (e + 1);
  grid_loc_query *lq = (grid_loc_query *) (gh + 1);
  
  if (gh->type != grid_hdr::GRID_LOC_QUERY) {
    click_chatter("LocQueryResponder %s: received unexpected Grid packet type %s; is the configuration wrong?",
		  id().cc(), grid_hdr::type_string(gh->type).cc());
    p->kill();
    return 0;
  }

  if (lq->dst_ip != (unsigned int) _ip) {
    click_chatter("LocQueryResponder %s: received location query for someone else (%s); is the configuration wrong?",
		  id().cc(), IPAddress(lq->dst_ip).s().cc());
    p->kill();
    return 0;
  }

  // ignore queries that are old
  unsigned int seq_no = ntohl(lq->seq_no);
  unsigned int *old_seq = _query_seqs.findp(gh->ip);
  if (old_seq && *old_seq >= seq_no) {
    p->kill();
    return 0;
  }
  _query_seqs.insert(gh->ip, seq_no);

  // construct the response
  WritablePacket *q = Packet::make(sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap));
  memset(q->data(), 0, q->length());
  e = (click_ether *) q->data();
  grid_hdr *ngh = (grid_hdr *) (e + 1);
  grid_nbr_encap *nb = (grid_nbr_encap *) (ngh + 1);

  ngh->hdr_len = sizeof(grid_hdr);
  ngh->type = grid_hdr::GRID_LOC_REPLY;
  ngh->ip = ngh->tx_ip = _ip;
  ngh->total_len = htons(q->length() - sizeof(click_ether));

  nb->dst_ip = gh->ip;
  nb->dst_loc = gh->loc;
  nb->dst_loc_err = gh->loc_err; // don't need to convert, already in network byte order
  nb->dst_loc_good = gh->loc_good;
  nb->hops_travelled = 0;

  p->kill();
  return(q);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LocQueryResponder)

