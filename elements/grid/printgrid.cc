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
#include <click/click_ether.h>
#include "grid.hh"

PrintGrid::PrintGrid()
  : Element(1, 1)
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
PrintGrid::configure(const Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpOptional,
		  cpString, "label", &_label,
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
  if (nb->dst_loc_good) {
    char buf[50];
    snprintf(buf, 50, "dst_lat=%.5f dst_lon=%.5f ", nb->dst_loc.lat(), nb->dst_loc.lon());
    line += buf;
    line += "dst_loc_err=" + String(ntohs(nb->dst_loc_err)) + " ";
  }
  else 
    line += "bad-dst-loc";
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
  /*  EtherAddress deth = EtherAddress(eh->ether_dhost); */
#if 0
  EtherAddress seth = EtherAddress(eh->ether_shost);
#endif

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));

#if 0
  IPAddress xip = IPAddress((unsigned) gh->ip);
#endif

  String type = grid_hdr::type_string(gh->type);

  String line("PrintGrid ");
  line += id() + " ";
  if (_label[0] != 0)
    line += _label + " ";
  line += ": " + type + " ";
  line += "hdr_len=" + String((unsigned int) gh->hdr_len) + " ";

  // packet originator info
  line += "ip=" + IPAddress(gh->ip).s() + " ";
  if (gh->loc_good) {
    char buf[50];
    snprintf(buf, 50, "lat=%.5f lon=%.5f ", gh->loc.lat(), gh->loc.lon());
    line += buf;
    line += "loc_err=" + String(ntohs(gh->loc_err)) + " ";
  }
  else
    line += "bad-loc ";
  line += "loc_seq_no=" + String(ntohl(gh->loc_seq_no)) + " ";
  
  // packet transmitter info
  line += "tx_ip=" + IPAddress(gh->tx_ip).s() + " ";
  if (gh->tx_loc_good) {
    char buf[50];
    snprintf(buf, 50, "tx_lat=%.5f tx_lon=%.5f ", gh->tx_loc.lat(), gh->tx_loc.lon());
    line += buf;
    line += "tx_loc_err=" + String(ntohs(gh->tx_loc_err)) + " ";
  }
  else
    line += "bad-tx-loc ";
  line += "tx_loc_seq_no=" + String(ntohl(gh->tx_loc_seq_no)) + " ";

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
    line += "age=" + String(ntohl(gh2->age)) + " ";
    line += "num_nbrs=" + String((unsigned int) gh2->num_nbrs);
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
  
#if 0
  if(gh->type == grid_hdr) {
    grid_hello *h = (grid_hello *) (gh + 1);
    grid_nbr_entry *na = (grid_nbr_entry *) (h + 1);
    buf = new char [h->num_nbrs * 100 + 1];
    assert(buf);
    snprintf(buf, h->num_nbrs*100 + 1, "%d ", h->num_nbrs);
    int i;
    for(i = 0; i < h->num_nbrs; i++){
      char tmp[100];
      snprintf(tmp, sizeof(buf), "%s %s %d %f %f ",
              IPAddress(na[i].ip).s().cc(),
              IPAddress(na[i].next_hop_ip).s().cc(),
              na[i].num_hops,
              na[i].loc.lat(),
              na[i].loc.lon());
      strcat(buf, tmp);
    }
  }

  click_chatter("PrintGrid%s%s : %s %s %s %.4f %.4f %s",
                _label.cc()[0] ? " " : "",
                _label.cc(),
                seth.s().cc(),
                type.cc(),
                xip.s().cc(),
                gh->loc.lat(),
                gh->loc.lon(),
                buf ? buf : "");
  
  if(buf)
    delete buf;
#endif;

  click_chatter("%s", line.cc());

  return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(PrintGrid)
