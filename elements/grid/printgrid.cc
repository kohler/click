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
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include <elements/grid/grid.hh>
#include <elements/grid/timeutils.hh>
#include <elements/grid/printgrid.hh>
#include <elements/grid/linkstat.hh>
CLICK_DECLS

PrintGrid::PrintGrid()
  : _print_routes(false), _print_probe_entries(false),
    _verbose(true), _timestamp(false), _print_eth(false)
{
  _label = "";
}

PrintGrid::~PrintGrid()
{
}

int
PrintGrid::configure(Vector<String> &conf, ErrorHandler* errh)
{
    return Args(conf, this, errh)
	.read_p("LABEL", _label)
	.read("SHOW_ROUTES", _print_routes)
	.read("SHOW_PROBE_CONTENTS", _print_probe_entries)
	.read("VERBOSE", _verbose)
	.read("TIMESTAMP", _timestamp)
	.read("PRINT_ETH", _print_eth)
	.complete();
}

String
PrintGrid::encap_to_string(const grid_nbr_encap *nb) const
{
  String line;
  line += "hops_travelled=" + String((unsigned int) nb->hops_travelled) + " ";
  line += "dst=" + IPAddress(nb->dst_ip).unparse() + " ";
  if (_verbose) {
#ifndef SMALL_GRID_HEADERS
    if (nb->dst_loc_good) {
      char buf[50];
      snprintf(buf, 50, "dst_loc=%s ", nb->dst_loc.unparse().c_str());
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
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID && ntohs(eh->ether_type) != LinkStat::ETHERTYPE_LINKSTAT) {
    click_chatter("PrintGrid %s%s%s : not a Grid packet",
		  name().c_str(),
		  _label.c_str()[0] ? " " : "",
		  _label.c_str());
    return p;
  }

  if (ntohs(eh->ether_type) == LinkStat::ETHERTYPE_LINKSTAT) {
    print_ether_linkstat(p);
    return p;
  }

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));

  String type = grid_hdr::type_string(gh->type);

  StringAccum line;
  line << "PrintGrid ";
  if (_verbose)
      line << name() << " ";
  if (_label[0] != 0)
      line << _label << " ";
  if (_timestamp)
      line << p->timestamp_anno() << ' ';

  if (_print_eth) {
      line << EtherAddress(eh->ether_shost) << ' ' << EtherAddress(eh->ether_dhost) << ' ';
      line.snprintf(10, "%04hx ", ntohs(eh->ether_type));
  }

  line << ": " << type << " ";
  if (_verbose)
      line << "hdr_len="  << gh->hdr_len << " ";

  // packet originator info
  line << "ip=" << IPAddress(gh->ip).unparse() << " ";
  if (_verbose) {
    if (gh->loc_good)
	line << "loc=" << gh->loc.s() << " loc_err=" << ntohs(gh->loc_err) << ' ';
    else
	line << "bad-loc ";
    line << "loc_seq_no=" << ntohl(gh->loc_seq_no) << " ";
  }

  // packet transmitter info
  line << "tx_ip=" << IPAddress(gh->tx_ip) << " ";
  if (_verbose) {
    if (gh->tx_loc_good)
	line << "tx_loc=" << gh->tx_loc.s() << " tx_loc_err=" << ntohs(gh->tx_loc_err) << ' ';
    else
	line << "bad-tx-loc ";
    line << "tx_loc_seq_no=" << ntohl(gh->tx_loc_seq_no) << " ";
  }

  if (_verbose)
      line << "pkt_len=" << ntohs(gh->total_len) << " ";

  line << "** ";

  grid_hello *gh2 = 0;
  grid_nbr_encap *nb = 0;
  grid_loc_query *lq = 0;
  grid_route_probe *rp = 0;
  grid_route_reply *rr = 0;

  switch (gh->type) {

  case grid_hdr::GRID_LR_HELLO:
    gh2 = (grid_hello *) (gh + 1);
    line << "seq_no=" << ntohl(gh2->seq_no) << " ";
    if (_verbose)
	line << "age=" << ntohl(gh2->age) << " ";
    line << "num_nbrs=" << (unsigned int) gh2->num_nbrs;
    if (_print_routes)
	line << get_entries(gh2);
    break;

  case grid_hdr::GRID_NBR_ENCAP:
  case grid_hdr::GRID_LOC_REPLY:
    nb = (grid_nbr_encap *) (gh + 1);
    line << encap_to_string(nb);
    break;

  case grid_hdr::GRID_LOC_QUERY:
    lq = (grid_loc_query *) (gh + 1);
    line << "dst_ip=" << IPAddress(lq->dst_ip) << " "
	 << "seq_no=" << ntohl(lq->seq_no);
    break;

  case grid_hdr::GRID_ROUTE_PROBE:
    nb = (grid_nbr_encap *) (gh + 1);
    line << encap_to_string(nb);
    rp = (grid_route_probe *) (nb + 1);
    line << " nonce=" << ntohl(rp->nonce);
    break;

  case grid_hdr::GRID_ROUTE_REPLY:
    nb = (grid_nbr_encap *) (gh + 1);
    line << encap_to_string(nb);
    rr = (grid_route_reply *) (nb + 1);
    line << " nonce=" << ntohl(rr->nonce)
	 << " probe_dest=" << IPAddress(rr->probe_dest)
	 << " reply_hop=" << (unsigned int) rr->reply_hop;
    break;

  case grid_hdr::GRID_LINK_PROBE: {
    grid_link_probe *lp = (grid_link_probe *) (gh + 1);
    line << " seq_no=" << ntohl(lp->seq_no)
	 << " period=" << ntohl(lp->period)
	 << " tau=" << ntohl(lp->tau)
	 << " num_links=" << ntohl(lp->num_links);
    if (_print_probe_entries)
	line << get_probe_entries(lp);
    break;
  }

  default:
    line << "Unknown grid header type " << (int) gh->type;
  }

  click_chatter("%s", line.c_str());

  return p;
}

void
PrintGrid::print_ether_linkstat(Packet *p) const
{
  StringAccum line;
  line << "PrintGrid ";
  if (_verbose)
    line << name() << " ";
  if (_label[0] != 0)
    line << _label << " ";
  if (_timestamp)
      line << p->timestamp_anno();

  unsigned min_sz = sizeof(click_ether) + LinkStat::link_probe::size;
  if (p->length() < min_sz) {
    line << "LinkStat packet is too small";
    click_chatter("%s", line.c_str());
    return;
  }

  if (_print_eth) {
    click_ether *eh = (click_ether *) p->data();
    char buf[100];
    snprintf(buf, sizeof(buf), "%s %s %04hx ",
	     EtherAddress(eh->ether_shost).unparse().c_str(), EtherAddress(eh->ether_dhost).unparse().c_str(),
	     ntohs(eh->ether_type));
    line << buf;
  }

  line << ": ETHER_LINK_PROBE ";

  LinkStat::link_probe lp(p->data() + sizeof(click_ether));
  if (LinkStat::link_probe::calc_cksum(p->data() + sizeof(click_ether)) != 0) {
    line << "Bad checksum";
    click_chatter("%s", line.c_str());
    return;
  }

  line << "psz=" << lp.psz << " num_links=" << lp.num_links;

  if (p->length() < lp.psz)
    line << " (short packet) ";

  unsigned int max_entries = (p->length() - sizeof(click_ether) - LinkStat::link_probe::size) / LinkStat::link_entry::size;
  unsigned int num_entries = lp.num_links;
  if (num_entries > max_entries) {
    line << " (truncated to " << max_entries << " links)";
    num_entries = max_entries;
  }

  if (_print_probe_entries) {
    const unsigned char *d = p->data() + sizeof(click_ether) + LinkStat::link_probe::size;
    for (unsigned i = 0; i < num_entries; i++, d += LinkStat::link_entry::size) {
      LinkStat::link_entry le(d);
      line << "\n\t" << le.eth << " num_rx=" << le.num_rx;
    }
  }

  click_chatter("%s", line.c_str());
}

String
PrintGrid::get_probe_entries(const grid_link_probe *lp) const
{
  StringAccum sa;
  grid_link_entry *le = (grid_link_entry *) (lp + 1);
  for (unsigned i = 0; i < ntohl(lp->num_links); i++, le++) {
    sa << "\n\t" << IPAddress(le->ip);
    sa << " num_rx=" << ntohl(le->num_rx);
#ifndef SMALL_GRID_PROBES
    sa << " period=" << ntohl(le->period);
    sa << " last_seq_no=" << ntohl(le->last_seq_no);
    sa << " last_rx_time=" << ntoh(le->last_rx_time);
    unsigned pct = 0;
    if (ntohl(le->period > 0)) {
      unsigned num_expected = ntohl(lp->tau) / ntohl(le->period);
      if (num_expected > 0)
	pct = 100 * htonl(le->num_rx) / num_expected;
    }
    sa << " pct=" << pct;
#endif
  }

  return sa.take_string();
}

String
PrintGrid::get_entries(const grid_hello *gh) const
{
  StringAccum ret;
  char *cp = (char *) (gh + 1);
  for (int i = 0; i < gh->num_nbrs; i++) {
    grid_nbr_entry *na = (grid_nbr_entry *) (cp + gh->nbr_entry_sz * i);
    ret << "\n\tip=" << IPAddress(na->ip)
	<< " next=" << IPAddress(na->next_hop_ip)
	<< " hops=" << (int) na->num_hops
	<< " seq=" << (unsigned) ntohl(na->seq_no) << ' ';

    if (na->metric_valid)
	ret << "metric=" << (unsigned) ntohl(na->metric);
    else
	ret << "bad-metric";

    if (_verbose) {
	ret << " gw=" << (na->is_gateway ? "yes" : "no")
	    << " ttl=" << ntohl(na->ttl);
      if (na->loc_good)
	  ret << " loc=" << na->loc.s() << " loc_err=" << (unsigned short) na->loc_err;
      else
	  ret << " bad-loc";
    }
  }
  return ret.take_string();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintGrid)
