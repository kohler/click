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
       T_FIRST_TIMESTAMP, T_COUNT, T_LINK, T_DIRECTION, T_AGGREGATE };

namespace IPSummaryDump {

static bool anno_extract(PacketDesc& d, int thunk)
{
    Packet *p = d.p;
    switch (thunk & ~B_TYPEMASK) {
      case T_TIMESTAMP:
	d.v = p->timestamp_anno().sec();
	d.v2 = p->timestamp_anno().nsec();
	return true;
      case T_TIMESTAMP_SEC:
	d.v = p->timestamp_anno().sec();
	return true;
      case T_TIMESTAMP_USEC:
	d.v = p->timestamp_anno().usec();
	return true;
      case T_TIMESTAMP_USEC1: {
#if HAVE_INT64_TYPES
	  uint64_t v3 = ((uint64_t)p->timestamp_anno().sec() * 1000000) + p->timestamp_anno().usec();
	  d.v = v3 >> 32;
	  d.v2 = v3;
	  return true;
#else
	  // XXX silently output garbage if 64-bit ints not supported
	  d.v = 0;
	  d.v2 = (p->timestamp_anno().sec() * 1000000) + p->timestamp_anno().usec();
	  return true;
#endif
      }
      case T_FIRST_TIMESTAMP:
	d.v = FIRST_TIMESTAMP_ANNO(p).sec();
	d.v2 = FIRST_TIMESTAMP_ANNO(p).nsec();
	return true;
      case T_COUNT:
	d.v = 1 + EXTRA_PACKETS_ANNO(p);
	return true;
      case T_LINK:
      case T_DIRECTION:
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
      case T_FIRST_TIMESTAMP:
	*d.sa << Timestamp::make_nsec(d.v, d.v2);
	break;
      case T_TIMESTAMP_USEC1:
#if HAVE_INT64_TYPES
	*d.sa << (((uint64_t)d.v) << 32) | d.v2;
#else
	*d.sa << d.v2;
#endif
	break;
      case T_DIRECTION:
	if (d.v == 0)
	    *d.sa << '>';
	else if (d.v == 1)
	    *d.sa << '<';
	else
	    *d.sa << d.v;
	break;
    }
}

#ifdef i386
# define PUT4(p, d)	*reinterpret_cast<uint32_t *>((p)) = htonl((d))
#else
# define PUT4(p, d)	do { (p)[0] = (d)>>24; (p)[1] = (d)>>16; (p)[2] = (d)>>8; (p)[3] = (d); } while (0)
#endif

static void timestamp_outb(const PacketDesc& d, bool, int)
{
    char* c = d.sa->extend(8);
    PUT4(c, d.v);
    PUT4(c + 4, d.v2 / 1000);
}

void anno_register_unparsers()
{
    register_unparser("timestamp", T_TIMESTAMP | B_8, 0, anno_extract, anno_outa, timestamp_outb);
    register_unparser("ntimestamp", T_TIMESTAMP | B_8, 0, anno_extract, anno_outa, outb);
    register_unparser("ts_sec", T_TIMESTAMP_SEC | B_4, 0, anno_extract, num_outa, outb);
    register_unparser("ts_usec", T_TIMESTAMP_USEC | B_4, 0, anno_extract, num_outa, outb);
    register_unparser("ts_usec1", T_TIMESTAMP_USEC1 | B_8, 0, anno_extract, anno_outa, outb);
    register_unparser("first_timestamp", T_FIRST_TIMESTAMP | B_8, 0, anno_extract, anno_outa, timestamp_outb);
    register_unparser("first_ntimestamp", T_FIRST_TIMESTAMP | B_8, 0, anno_extract, anno_outa, outb);
    register_unparser("count", T_COUNT | B_4, 0, anno_extract, num_outa, outb);
    register_unparser("link", T_LINK | B_1, 0, anno_extract, num_outa, outb);
    register_unparser("direction", T_DIRECTION | B_1, 0, anno_extract, anno_outa, outb);
    register_unparser("aggregate", T_AGGREGATE | B_4, 0, anno_extract, num_outa, outb);
    
    register_synonym("utimestamp", "timestamp");
    register_synonym("ts", "utimestamp");
    register_synonym("sec", "ts_sec");
    register_synonym("usec", "ts_usec");
    register_synonym("usec1", "ts_usec1");
    register_synonym("first_utimestamp", "first_timestamp");
    register_synonym("first_ts", "first_utimestamp");
    register_synonym("pkt_count", "count");
    register_synonym("packet_count", "count");
    register_synonym("agg", "aggregate");
}

}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_Anno)
CLICK_ENDDECLS
