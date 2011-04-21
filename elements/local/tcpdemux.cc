/*
 * tcpdemux.{cc,hh} -- demultiplexes tcp flows
 * Benjie Chen
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/router.hh>
#include <click/error.hh>
#include "tcpdemux.hh"
CLICK_DECLS

TCPDemux::TCPDemux()
{
}

TCPDemux::~TCPDemux()
{
}

int
TCPDemux::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}


int
TCPDemux::find_flow(Packet *p)
{
  const click_ip *iph = p->ip_header();
  const click_tcp *tcph = p->tcp_header();

  // match (sa,sp,da,dp)
  IPFlowID fid(p);
  int *iptr = _flows.findp(fid.reverse());
  if (iptr)
    return *iptr;

  // match (0,0,da,dp)
  fid = IPFlowID(0, 0, iph->ip_dst, tcph->th_dport);
  iptr = _flows.findp(fid.reverse());
  if (iptr)
    return *iptr;

  // match (0,0,0,dp)
  fid = IPFlowID(0, 0, 0, tcph->th_dport);
  iptr = _flows.findp(fid.reverse());
  if (iptr)
    return *iptr;

  // match (0,0,da,0)
  fid = IPFlowID(0, 0, iph->ip_dst, 0);
  iptr = _flows.findp(fid.reverse());
  if (iptr)
    return *iptr;

  // match (0,0,0,0)
  fid = IPFlowID(0, 0, 0, 0);
  iptr = _flows.findp(fid);
  if (iptr)
    return *iptr;

  return -1;
}

void
TCPDemux::push(int, Packet *p)
{
  int port = find_flow(p);
  if (port < 0 || port > noutputs()) {
    click_chatter("reject packet from unknown flow");
    p->kill();
  }
  else
    output(port).push(p);
}

bool
TCPDemux::add_flow(IPAddress sa, unsigned short sp,
                   IPAddress da, unsigned short dp, unsigned port)
{
  if (_flows.findp(IPFlowID(sa, sp, da, dp)))
    return false;
  else
    return _flows.insert(IPFlowID(sa, sp, da, dp), port);
}


bool
TCPDemux::remove_flow(IPAddress sa, unsigned short sp,
                      IPAddress da, unsigned short dp)
{
  return _flows.remove(IPFlowID(sa, sp, da, dp));
}

EXPORT_ELEMENT(TCPDemux)
CLICK_ENDDECLS
