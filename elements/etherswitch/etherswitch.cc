/*
 * etherswitch.{cc,hh} -- learning, forwarding Ethernet bridge
 * John Jannotti
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
#include <click/package.hh>
#include "etherswitch.hh"

#include <click/click_ether.h>
#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <click/bitvector.hh>

EtherSwitch::EtherSwitch()
  : _table(0), _timeout(300)
{
  MOD_INC_USE_COUNT;
}

EtherSwitch::~EtherSwitch()
{
  MOD_DEC_USE_COUNT;
  for (Table::Iterator iter = _table.first(); iter; iter++)
    delete iter.value();
  _table.clear();
}

EtherSwitch::AddrInfo::AddrInfo(int p, const timeval& s)
  : port(p), stamp(s)
{
}

void
EtherSwitch::notify_ninputs(int n)
{
  set_ninputs(n);
  set_noutputs(n);
}


EtherSwitch *
EtherSwitch::clone() const
{
  return new EtherSwitch;
}

void
EtherSwitch::broadcast(int source, Packet *p)
{
  int n = noutputs();
  int sent = 0;
  for (int i = 0; i < n; i++)
    if (i != source) {
      Packet *pp = (sent < n - 2 ? p->clone() : p);
      output(i).push(pp);
      sent++;
    }
}

void
EtherSwitch::push(int source, Packet *p)
{
  click_ether* e = (click_ether*) p->data();

  timeval t;
  click_gettimeofday(&t);

  EtherAddress src = EtherAddress(e->ether_shost);
  EtherAddress dst = EtherAddress(e->ether_dhost);

#if 0
  click_chatter("Got a packet %p on %d at %d.%06d with src %s and dst %s",
	      p, source, t.tv_sec, t.tv_usec,
              src.s().cc(),
              dst.s().cc());
#endif

  if (AddrInfo* src_info = _table[src]) {
    src_info->port = source;	// It's possible that it has changed.
    src_info->stamp = t;
  } else {
    _table.insert(src, new AddrInfo(source, t));
  }
  
  int outport = -1;		// Broadcast
  
  // Set outport if dst is unicast, we have info about it, and the
  // info is still valid.
  if (!dst.is_group()) {
    if (AddrInfo* dst_info = _table[dst]) {
      //      click_chatter("Got a packet for a known dst on %d to %d\n",
      //		  source, dst_info->port);
      t.tv_sec -= _timeout;
      if (timercmp(&dst_info->stamp, &t, >)) {
	outport = dst_info->port;
      }
    }
  }

  if (outport < 0)
    broadcast(source, p);
  else if (outport == source)	// Don't send back out on same interface
    p->kill();
  else				// forward
    output(outport).push(p);
}

String
EtherSwitch::read_table(Element* f, void *) {
  EtherSwitch* sw = (EtherSwitch*)f;
  String s;
  for (Table::Iterator iter = sw->_table.first(); iter; iter++)
    s += iter.key().s() + " " + String(iter.value()->port) + "\n";
  return s;
}

void
EtherSwitch::add_handlers()
{
  add_read_handler("table", read_table, 0);
}

EXPORT_ELEMENT(EtherSwitch)

#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, EtherSwitch::AddrInfo*>;
#endif
