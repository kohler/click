/*
 * SRDestCache.{cc,hh}
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "srdestcache.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
CLICK_DECLS



SRDestCache::SRDestCache()
{

}

SRDestCache::~SRDestCache()
{
}

int
SRDestCache::configure (Vector<String> &conf, ErrorHandler *errh)
{
	return cp_va_parse(conf, this, errh,
			   cpKeywords,
			   cpEnd);	
}

void
SRDestCache::push(int port, Packet *p_in)
{
	if (port == 0) {
		IPAddress client = p_in->dst_ip_anno();
		CacheEntry *c = _cache.findp(client);
		if (!c) {
			p_in->kill();
			return;
		} else {
			p_in->set_dst_ip_anno(c->_ap);
		}
	} else {
		const click_ip *ip = p_in->ip_header();
		IPAddress client = ip->ip_src;
		IPAddress ap = MISC_IP_ANNO(p_in);
		CacheEntry *c = _cache.findp(client);
		if (!c) {
			_cache.insert(client, CacheEntry(client, ap));
			c = _cache.findp(client);
		}
		c->_ap = ap;
		c->_last = Timestamp::now();
	}
	output(port).push(p_in);
}

enum {H_CACHE};
String
SRDestCache::read_param(Element *e, void *vparam)
{
  SRDestCache *d = (SRDestCache *) e;
  switch ((int)vparam) {
  case H_CACHE: {
	  StringAccum sa;
	  struct timeval now;
	  click_gettimeofday(&now);
	  
	  for(FTIter iter = d->_cache.begin(); iter; iter++) {
		  CacheEntry c = iter.value();
		  sa << c._client << " " << c._ap << " age " << Timestamp::now() - c._last << "\n";
	  }
	  
	  return sa.take_string();
  }
  default:
    return "";
  }
}
void
SRDestCache::add_handlers()
{
  add_read_handler("cache", read_param, (void *) H_CACHE);
}

// generate Vector template instance
#include <click/hashmap.cc>
CLICK_ENDDECLS
EXPORT_ELEMENT(SRDestCache)
