/*
 * setipaddress.{cc,hh} -- element sets destination address annotation
 * to a random IP address
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "setrandipaddress.hh"
#include <click/args.hh>
CLICK_DECLS

SetRandIPAddress::SetRandIPAddress()
{
  _max = -1;
  _addrs = 0;
}

SetRandIPAddress::~SetRandIPAddress()
{
}

int
SetRandIPAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _max = -1;
    int ret = Args(conf, this, errh)
	.read_mp("PREFIX", IPPrefixArg(true), _ip, _mask)
	.read_p("LIMIT", _max).complete();

    if(_max >= 0){
	_addrs = new IPAddress [_max] ();

	int i;
	for(i = 0; i < _max; i++)
	    _addrs[i] = pick();
    }

    return(ret);
}

IPAddress
SetRandIPAddress::pick()
{
  uint32_t x;
  uint32_t mask = (uint32_t) _mask;

  x = (click_random() & ~mask) | ((uint32_t)_ip & mask);

  return(IPAddress(x));
}

Packet *
SetRandIPAddress::simple_action(Packet *p)
{
  IPAddress ipa;

  if(_addrs && _max > 0){
    ipa = _addrs[click_random(0, _max - 1)];
  } else {
    ipa = pick();
  }

  p->set_dst_ip_anno(ipa);

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetRandIPAddress)
ELEMENT_MT_SAFE(SetRandIPAddress)
