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
#include <click/confparse.hh>
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
	d.u32[0] = p->timestamp_anno().sec();
	d.u32[1] = p->timestamp_anno().nsec();
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
	  d.u32[1] = v3;
	  d.u32[0] = v3 >> 32;
	  return true;
#else
	  // XXX silently output garbage if 64-bit ints not supported
	  d.u32[1] = (p->timestamp_anno().sec() * 1000000) + p->timestamp_anno().usec();
	  d.u32[0] = 0;
	  return true;
#endif
      }
      case T_FIRST_TIMESTAMP:
	d.u32[0] = FIRST_TIMESTAMP_ANNO(p).sec();
	d.u32[1] = FIRST_TIMESTAMP_ANNO(p).nsec();
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

static void anno_inject(PacketOdesc& d, int thunk)
{
    WritablePacket *p = d.p;
    switch (thunk & ~B_TYPEMASK) {
    case T_TIMESTAMP:
	p->set_timestamp_anno(Timestamp::make_nsec(d.u32[0], d.u32[1]));
	break;
    case T_TIMESTAMP_SEC:
	p->timestamp_anno().set_sec(d.v);
	break;
    case T_TIMESTAMP_USEC:
	p->timestamp_anno().set_subsec(Timestamp::usec_to_subsec(d.v));
	break;
    case T_TIMESTAMP_USEC1: {
#if HAVE_INT64_TYPES
	uint64_t v3 = ((uint64_t) d.u32[1] << 32) | d.u32[0];
	p->set_timestamp_anno(Timestamp::make_usec(v3 / 1000000, v3 % 1000000));
	break;
#else
	// XXX silently output garbage if 64-bit ints not supported
	p->set_timestamp_anno(Timestamp::make_usec(d.u32[0] / 1000000, d.u32[0] % 1000000));
	break;
#endif
    }
    case T_FIRST_TIMESTAMP:
	SET_FIRST_TIMESTAMP_ANNO(p, Timestamp::make_nsec(d.u32[0], d.u32[1]));
	break;
    case T_COUNT:
	SET_EXTRA_PACKETS_ANNO(p, d.v ? d.v - 1 : 0);
	break;
    case T_LINK:
    case T_DIRECTION:
	SET_PAINT_ANNO(p, d.v);
	break;
    case T_AGGREGATE:
	SET_AGGREGATE_ANNO(p, d.v);
	break;
    }
}

static void anno_outa(const PacketDesc& d, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
      case T_TIMESTAMP:
      case T_FIRST_TIMESTAMP:
	*d.sa << Timestamp::make_nsec(d.u32[0], d.u32[1]);
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

static bool anno_ina(PacketOdesc& d, const String &s, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
    case T_TIMESTAMP:
    case T_FIRST_TIMESTAMP: {
	Timestamp ts;
	if (cp_time(s, &ts)) {
	    d.u32[0] = ts.sec();
	    d.u32[1] = ts.nsec();
	    return true;
	}
	break;
    }
    case T_LINK:
    case T_DIRECTION:
	if (s.equals(">", 1)) {
	    d.v = 0;
	    return true;
	} else if (s.equals("<", 1)) {
	    d.v = 1;
	    return true;
	} else
	    return cp_integer(s, &d.v);
    }
    return false;
}

#ifdef i386
# define PUT4(p, d)	*reinterpret_cast<uint32_t *>((p)) = htonl((d))
# define GET4(p)	ntohl(*reinterpret_cast<const uint32_t *>((p)))
#else
# define PUT4(p, d)	do { (p)[0] = (d)>>24; (p)[1] = (d)>>16; (p)[2] = (d)>>8; (p)[3] = (d); } while (0)
# define GET4(p)	((p)[0]<<24 | (p)[1]<<16 | (p)[2]<<8 | (p)[3])
#endif

static void utimestamp_outb(const PacketDesc& d, bool, int)
{
    char* c = d.sa->extend(8);
    PUT4(c, d.u32[0]);
    PUT4(c + 4, d.u32[1] / 1000);
}

static const uint8_t *utimestamp_inb(PacketOdesc& d, const uint8_t* s, const uint8_t *ends, int)
{
    if (s + 7 <= ends)
	return ends;
    d.u32[0] = GET4(s);
    d.u32[1] = GET4(s + 4) * 1000;
    return s + 8;
}

void anno_register_unparsers()
{
    register_field("timestamp", T_TIMESTAMP | B_8, 0, order_anno,
		   anno_extract, anno_inject, anno_outa, anno_ina, utimestamp_outb, utimestamp_inb);
    register_field("ntimestamp", T_TIMESTAMP | B_8, 0, order_anno,
		   anno_extract, anno_inject, anno_outa, anno_ina, outb, inb);
    register_field("ts_sec", T_TIMESTAMP_SEC | B_4, 0, order_anno,
		   anno_extract, anno_inject, num_outa, num_ina, outb, inb);
    register_field("ts_usec", T_TIMESTAMP_USEC | B_4, 0, order_anno,
		   anno_extract, anno_inject, num_outa, num_ina, outb, inb);
    register_field("ts_usec1", T_TIMESTAMP_USEC1 | B_8, 0, order_anno,
		   anno_extract, anno_inject, num_outa, num_ina, outb, inb);
    register_field("first_timestamp", T_FIRST_TIMESTAMP | B_8, 0, order_anno,
		   anno_extract, anno_inject, anno_outa, anno_ina, utimestamp_outb, utimestamp_inb);
    register_field("first_ntimestamp", T_FIRST_TIMESTAMP | B_8, 0, order_anno,
		   anno_extract, anno_inject, anno_outa, anno_ina, outb, inb);
    register_field("count", T_COUNT | B_4, 0, order_anno,
		   anno_extract, anno_inject, num_outa, num_ina, outb, inb);
    register_field("link", T_LINK | B_1, 0, order_anno,
		   anno_extract, anno_inject, num_outa, anno_ina, outb, inb);
    register_field("direction", T_DIRECTION | B_1, 0, order_anno,
		   anno_extract, anno_inject, anno_outa, anno_ina, outb, inb);
    register_field("aggregate", T_AGGREGATE | B_4, 0, order_anno,
		   anno_extract, anno_inject, num_outa, num_ina, outb, inb);
    
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
