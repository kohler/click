// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_general.{cc,hh} -- general IP summary dump unparsers
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
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

#include "ipsumdumpinfo.hh"
#include <click/packet.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

enum { T_TIMESTAMP, T_TIMESTAMP_SEC, T_TIMESTAMP_USEC, T_TIMESTAMP_USEC1,
       T_FIRST_TIMESTAMP, T_COUNT, T_LINK, T_AGGREGATE };

namespace IPSummaryDump {

static bool anno_extract(PacketDesc& d, int thunk)
{
    Packet *p = d.p;
    switch (thunk & ~B_TYPEMASK) {
      case T_TIMESTAMP:
	d.v = p->timestamp_anno().tv_sec;
	d.v2 = p->timestamp_anno().tv_usec;
	return true;
      case T_TIMESTAMP_SEC:
	d.v = p->timestamp_anno().tv_sec;
	return true;
      case T_TIMESTAMP_USEC:
	d.v = p->timestamp_anno().tv_usec;
	return true;
      case T_TIMESTAMP_USEC1: {
#if HAVE_INT64_TYPES
	  uint64_t v3 = ((uint64_t)p->timestamp_anno().tv_sec * 1000000) + p->timestamp_anno().tv_usec;
	  d.v = v3 >> 32;
	  d.v2 = v3;
	  return true;
#else
	  // XXX silently output garbage if 64-bit ints not supported
	  d.v = 0;
	  d.v2 = (p->timestamp_anno().tv_sec * 1000000) + p->timestamp_anno().tv_usec;
	  return true;
#endif
      }
      case T_FIRST_TIMESTAMP:
	d.v = FIRST_TIMESTAMP_ANNO(p).tv_sec;
	d.v2 = FIRST_TIMESTAMP_ANNO(p).tv_usec;
	return true;
      case T_COUNT:
	d.v = 1 + EXTRA_PACKETS_ANNO(p);
	return true;
      case T_LINK:
	d.v = PAINT_ANNO(p);
	return true;
      case T_AGGREGATE:
	d.v = AGGREGATE_ANNO(p);
	return true;
      default:
	return false;
    }
}

static void anno_outa(const PacketDesc& d, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
      case T_TIMESTAMP:
      case T_FIRST_TIMESTAMP: {
	  timeval tv = make_timeval(d.v, d.v2);
	  *d.sa << tv;
	  break;
      }
      case T_TIMESTAMP_USEC1:
#if HAVE_INT64_TYPES
	*d.sa << (((uint64_t)d.v) << 32) | d.v2;
#else
	*d.sa << d.v2;
#endif
	break;
      case T_LINK:
	if (d.v == 0)
	    *d.sa << '>';
	else if (d.v == 1)
	    *d.sa << '<';
	else
	    *d.sa << d.v;
	break;
    }
}

void anno_register_unparsers()
{
    register_parser("timestamp", T_TIMESTAMP | B_8, 0, anno_extract, anno_outa, outb);
    register_parser("ts_sec", T_TIMESTAMP_SEC | B_4, 0, anno_extract, num_outa, outb);
    register_parser("ts_usec", T_TIMESTAMP_USEC | B_4, 0, anno_extract, num_outa, outb);
    register_parser("ts_usec1", T_TIMESTAMP_USEC1 | B_8, 0, anno_extract, anno_outa, outb);
    register_parser("first_timestamp", T_FIRST_TIMESTAMP | B_8, 0, anno_extract, anno_outa, outb);
    register_parser("count", T_COUNT | B_4, 0, anno_extract, num_outa, outb);
    register_parser("link", T_LINK | B_1, 0, anno_extract, anno_outa, outb);
    register_parser("aggregate", T_AGGREGATE | B_4, 0, anno_extract, num_outa, outb);
    
    register_synonym("ts", "timestamp");
    register_synonym("sec", "ts_sec");
    register_synonym("usec", "ts_usec");
    register_synonym("usec1", "ts_usec1");
    register_synonym("first_ts", "first_timestamp");
    register_synonym("pkt_count", "count");
    register_synonym("packet_count", "count");
    register_synonym("direction", "link");
    register_synonym("agg", "aggregate");
}

}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_Anno)
CLICK_ENDDECLS
