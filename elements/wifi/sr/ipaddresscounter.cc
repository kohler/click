/*
 * ipaddresscounter.{cc,hh} -- 
 * John Bicket
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
#include <click/confparse.hh>
#include <click/error.hh>
#include "ipaddresscounter.hh"
#include "srpacket.hh"
#include <clicknet/ip.h>
CLICK_DECLS

IPAddressCounter::IPAddressCounter()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

IPAddressCounter::~IPAddressCounter()
{
  MOD_DEC_USE_COUNT;
}


int
IPAddressCounter::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  bool track_src = false;
  bool track_dst = false;

  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "USE_SRC", cpBool, "", &track_src,
		    "USE_DST", cpBool, "", &track_dst,
                    cpEnd);

  if (!(track_src ^ track_dst)) {
    return errh->error("exactly one of SRC or DST must be specified\n");
  }
  _track_src = track_src;
  return ret;

}
Packet *
IPAddressCounter::simple_action(Packet *p)
{
  const click_ip *cip = reinterpret_cast<const click_ip *>(p->data());
  unsigned plen = p->length();
  
  if (!cip) {
    return p;
  }
  
  IPAddress ip;
  
  if (_track_src) {
    ip = IPAddress(cip->ip_src);
  } else {
      ip = IPAddress(cip->ip_dst);
  }
  IPAddressInfo *nfo = _table.findp(ip);
  if (!nfo) {
    _table.insert(ip, IPAddressInfo(ip));
    nfo = _table.findp(ip);
      }
  nfo->_packets++;
  nfo->_bytes += plen;
  
  return (p);
}

String 
IPAddressCounter::stats() 
{
  StringAccum sa;
  struct timeval now;
  click_gettimeofday(&now);

  for (IPIter iter = _table.begin(); iter; iter++) {
    IPAddressInfo n = iter.value();
    sa << n._ip.s().cc();
    sa << " packets " << n._packets;
    sa << " kbytes " << n._bytes / 1024;
    sa << "\n";
  }
    
  return sa.take_string();
}
String
IPAddressCounter::read_param(Element *e, void *vparam)
{
  IPAddressCounter *f = (IPAddressCounter *) e;
  switch ((int)vparam) {
  case 0:		//stats	
    return f->stats();
  default:
    return "";
  }
}

int 
IPAddressCounter::write_param(const String &in_s, Element *e, void *vparam,
			 ErrorHandler *errh)
{
  IPAddressCounter *f = (IPAddressCounter *) e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case 0: {    // stats
    return errh->error("stats not implemented");
    break;
  }
  case 1: {	// clear
    f->_table.clear();
    break;
  }
  }
  return 0;
}
  void
IPAddressCounter::add_handlers()
{
  add_read_handler("stats", read_param, (void *) 0);
  add_write_handler("stats", write_param, (void *) 0);
  add_write_handler("reset", write_param, (void *) 1);
}
// generate Vector template instance
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<IPAddress, IPAddressCounter::IPAddressInfo>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(IPAddressCounter)
