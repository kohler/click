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
#include <click/confparse.hh>
CLICK_DECLS

SetRandIPAddress::SetRandIPAddress()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _max = -1;
  _addrs = 0;
}

SetRandIPAddress::~SetRandIPAddress()
{
  MOD_DEC_USE_COUNT;
}

int
SetRandIPAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int ret;

  _max = -1;
  ret = cp_va_parse(conf, this, errh,
                    cpIPAddressOrPrefix, "IP address/len", &_ip, &_mask,
                    cpOptional,
                    cpInteger, "MAX addresses", &_max,
                    0);

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

  x = (random() & ~mask) | ((uint32_t)_ip & mask);
  
  return(IPAddress(x));
}

Packet *
SetRandIPAddress::simple_action(Packet *p)
{
  IPAddress ipa;

  if(_addrs && _max > 0){
    ipa = _addrs[random() % _max];
  } else {
    ipa = pick();
  }

  p->set_dst_ip_anno(ipa);

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetRandIPAddress)
ELEMENT_MT_SAFE(SetRandIPAddress)
