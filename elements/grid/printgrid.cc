/*
 * printgrid.{cc,hh} -- print Grid packets, for debugging.
 * Robert Morris
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
#include "printgrid.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>

#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include "grid.hh"
CLICK_DECLS

PrintGrid::PrintGrid()
  : Element(1, 1), _print_routes(false), _verbose(true)
{
  MOD_INC_USE_COUNT;
  _label = "";
}

PrintGrid::~PrintGrid()
{
  MOD_DEC_USE_COUNT;
}

PrintGrid *
PrintGrid::clone() const
{
  return new PrintGrid;
}

int
PrintGrid::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpOptional,
		  cpString, "label", &_label,
		  cpKeywords,
		  "SHOW_ROUTES", cpBool, "print route entries in advertisments?", &_print_routes,
		  "VERBOSE", cpBool, "show more detail?", &_verbose,
		  "TIMESTAMP", cpBool, "print packet timestamps?", &_timestamp,
		  cpEnd) < 0)
    return -1;
  return(0);
}

String
PrintGrid::encap_to_string(grid_nbr_encap *nb)
{
  String line;
  line += "hops_travelled=" + String((unsigned int) nb->hops_travelled) + " ";
  line += "dst=" + IPAddress(nb->dst_ip).s() + " ";
  if (_verbose) {
#ifndef SMALL_GRID_HEADERS
    if (nb->dst_loc_good) {
      char buf[50];
      snprintf(buf, 50, "dst_loc=%s ", nb->dst_loc.s().cc());
      line += buf;
      line += "dst_loc_err=" + String(ntohs(nb->dst_loc_err)) + " ";
    }
    else 
      line += "bad-dst-loc";
#else
    line += "bad-dst-loc";
#endif
  }
  return line;
}

Packet *
PrintGrid::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("PrintGrid %s%s%s : not a Grid packet", 
		  id().cc(),
		  _label.cc()[0] ? " " : "",
		  _label.cc());
    return p;
  }

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));

  String type = grid_hdr::type_string(gh->type);

  String line("PrintGrid ");
  if (_verbose)
    line += id() + " ";
  if (_label[0] != 0)
    line += _label + " ";
  if (_timestamp) {
    char buf[30];
    snprintf(buf, sizeof(buf), "%ld.%06ld ",
	     p->timestamp_anno().tv_sec,
	     p->timestamp_anno().tv_usec);
    line += buf;
  }

  line += ": " + type + " ";
  if (_verbose)
    line += "hdr_len=" + String((unsigned int) gh->hdr_len) + " ";

  // packet originator info
  line += "ip=" + IPAddress(gh->ip).s() + " ";
  if (_verbose) {
    if (gh->loc_good) {
      char buf[50];
      snprintf(buf, 50, "loc=%s ", gh->loc.s().cc());
      line += buf;
      line += "loc_err=" + String(ntohs(gh->loc_err)) + " ";
    }
    else
      line += "bad-loc ";
    line += "loc_seq_no=" + String(ntohl(gh->loc_seq_no)) + " ";
  }
  
  // packet transmitter info
  line += "tx_ip=" + IPAddress(gh->tx_ip).s() + " ";
  if (_verbose) {
    if (gh->tx_loc_good) {
      char buf[50];
      snprintf(buf, 50, "tx_loc=%s ", gh->tx_loc.s().cc());
      line += buf;
      line += "tx_loc_err=" + String(ntohs(gh->tx_loc_err)) + " ";
    }
    else
      line += "bad-tx-loc ";
    line += "tx_loc_seq_no=" + String(ntohl(gh->tx_loc_seq_no)) + " ";
  }

  if (_verbose)
    line += "pkt_len=" + String(ntohs(gh->total_len)) + " ";

  line += "** ";

  grid_hello *gh2 = 0;
  grid_nbr_encap *nb = 0;
  grid_loc_query *lq = 0;
  grid_route_probe *rp = 0;
  grid_route_reply *rr = 0;

  switch (gh->type) {

  case grid_hdr::GRID_LR_HELLO:
    gh2 = (grid_hello *) (gh + 1);
    line += "seq_no=" + String(ntohl(gh2->seq_no)) + " ";
    if (_verbose)
      line += "age=" + String(ntohl(gh2->age)) + " ";
    line += "num_nbrs=" + String((unsigned int) gh2->num_nbrs);
    if (_print_routes)
      line += get_entries(gh2);
    break;

  case grid_hdr::GRID_NBR_ENCAP:
  case grid_hdr::GRID_LOC_REPLY:
    nb = (grid_nbr_encap *) (gh + 1);
    line += encap_to_string(nb);
    break;

  case grid_hdr::GRID_LOC_QUERY:
    lq = (grid_loc_query *) (gh + 1);
    line += "dst_ip=" + IPAddress(lq->dst_ip).s() + " ";
    line += "seq_no=" + String(ntohl(lq->seq_no));
    break;

  case grid_hdr::GRID_ROUTE_PROBE: 
    nb = (grid_nbr_encap *) (gh + 1);
    line += encap_to_string(nb);
    rp = (grid_route_probe *) (nb + 1);
    line += " nonce=" + String(ntohl(rp->nonce));
    break;

  case grid_hdr::GRID_ROUTE_REPLY: 
    nb = (grid_nbr_encap *) (gh + 1);
    line += encap_to_string(nb);
    rr = (grid_route_reply *) (nb + 1);
    line += " nonce=" + String(ntohl(rr->nonce));
    line += " probe_dest=" + IPAddress(rr->probe_dest).s();
    line += " reply_hop=" + String((unsigned int) rr->reply_hop);
    break;

  default:
    line += "Don't know how to print this header";
  }
  
  click_chatter("%s", line.cc());

  return p;
}


String
PrintGrid::get_entries(grid_hello *gh)
{
  String ret;
  char *cp = (char *) (gh + 1);
  for (int i = 0; i < gh->num_nbrs; i++) {
    grid_nbr_entry *na = (grid_nbr_entry *) (cp + gh->nbr_entry_sz * i);
    char buf[1024];
    snprintf(buf, sizeof(buf), "\n\tip=%s next=%s hops=%d seq=%lu ",
	     IPAddress(na->ip).s().cc(),
	     IPAddress(na->next_hop_ip).s().cc(),
	     (int) na->num_hops,
	     (unsigned long) ntohl(na->seq_no));
    ret += buf;

    if (na->metric_valid) {
      snprintf(buf, sizeof(buf), "metric=%lu", (unsigned long) ntohl(na->metric));
      ret += buf;
    }
    else
      ret += "bad-metric";

    if (_verbose) {
      ret += String(" gw=") + (na->is_gateway ? "yes" : "no");
      ret += String(" ttl=") + String(ntohl(na->ttl));
      if (na->loc_good) {
	snprintf(buf, sizeof(buf), " loc=%s loc_err=%us",
		 na->loc.s().cc(), ntohs(na->loc_err));
	ret += buf;
      }
      else
	ret += " bad-loc";
    }
  }
  return ret;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(PrintGrid)
