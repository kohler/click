/*
 * arptable.{cc,hh} -- Poor man's arp table
 * John Bicket
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "arptable.hh"
CLICK_DECLS

ARPTable::ARPTable()
  : Element(0, 0)
{
  MOD_INC_USE_COUNT;

  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);

}

ARPTable::~ARPTable()
{
  MOD_DEC_USE_COUNT;
}

ARPTable *
ARPTable::clone() const
{
  return new ARPTable;
}
void *
ARPTable::cast(const char *n)
{
  if (strcmp(n, "ARPTable") == 0)
    return (ARPTable *) this;
  else
    return 0;
}
int
ARPTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res;
  res = cp_va_parse(conf, this, errh,
		    cpKeywords, 
		    0);

  return res;
}

EtherAddress 
ARPTable::lookup(IPAddress ip)
{
  DstInfo *dst = _table.findp(ip);
  if (dst) {
    return dst->_eth;
  }
  return _bcast;
}

IPAddress
ARPTable::reverse_lookup(EtherAddress eth)
{
  IPAddress *ip = _rev_table.findp(eth);

  if (ip) {
    return _rev_table[eth];
  }
  return IPAddress();

}
void
ARPTable::insert(IPAddress ip, EtherAddress eth) 
{
  DstInfo *dst = _table.findp(ip);
  if (!dst) {
    _table.insert(ip, DstInfo(ip));
    dst = _table.findp(ip);
  }
  dst->_eth = eth;
  click_gettimeofday(&dst->_when);


  _rev_table.insert(eth, ip);
}
String
ARPTable::static_print_mappings(Element *e, void *)
{
  ARPTable *n = (ARPTable *) e;
  return n->print_mappings();
}

String
ARPTable::print_mappings() 
{
  struct timeval now;
  click_gettimeofday(&now);
  
  StringAccum sa;
  for (ARPIter iter = _table.begin(); iter; iter++) {
    DstInfo n = iter.value();
    struct timeval age = now - n._when;
    sa << n._ip.s().cc() << " ";
    sa << n._eth.s().cc() << " ";
    sa << "last_received: " << age << "\n";
  }
  return sa.take_string();
}
void
ARPTable::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("mappings", static_print_mappings, 0);

}
// generate Vector template instance
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<IPAddress, ARPTable::DstInfo>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(ARPTable)

