// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_anno.{cc,hh} -- IP summary dump (un)parsers for annotations
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

#include "ipsumdump_anno.hh"
#include <click/packet.hh>
#include <click/args.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

enum { T_TIMESTAMP, T_TIMESTAMP_SEC, T_TIMESTAMP_USEC, T_TIMESTAMP_USEC1,
       T_FIRST_TIMESTAMP, T_COUNT, T_LINK, T_DIRECTION, T_AGGREGATE,
       T_WIRE_LEN };

namespace IPSummaryDump {

static bool anno_extract(PacketDesc& d, const FieldWriter *f)
{
    const Packet *p = d.p;
    switch (f->user_data) {
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
	d.u32[0] = CONST_FIRST_TIMESTAMP_ANNO(p).sec();
	d.u32[1] = CONST_FIRST_TIMESTAMP_ANNO(p).nsec();
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
    case T_WIRE_LEN:
	d.v = p->length();
	return true;
      default:
	return false;
    }
}

static void anno_inject(PacketOdesc& d, const FieldReader *f)
{
    WritablePacket *p = d.p;
    switch (f->user_data) {
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
    case T_WIRE_LEN:
	d.want_len = d.v;
	break;
    }
}

static void anno_outa(const PacketDesc& d, const FieldWriter *f)
{
    switch (f->user_data) {
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

static bool anno_ina(PacketOdesc& d, const String &s, const FieldReader *f)
{
    switch (f->user_data) {
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
	    return IntArg().parse(s, d.v);
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

static void utimestamp_outb(const PacketDesc& d, bool, const FieldWriter *)
{
    char* c = d.sa->extend(8);
    PUT4(c, d.u32[0]);
    PUT4(c + 4, d.u32[1] / 1000);
}

static const uint8_t *utimestamp_inb(PacketOdesc& d, const uint8_t* s, const uint8_t *ends, const FieldReader *)
{
    if (s + 7 >= ends)
	return ends;
    d.u32[0] = GET4(s);
    d.u32[1] = GET4(s + 4) * 1000;
    return s + 8;
}

static const IPSummaryDump::FieldWriter anno_writers[] = {
    { "timestamp", B_8, T_TIMESTAMP,
      0, anno_extract, anno_outa, utimestamp_outb },
    { "ntimestamp", B_8, T_TIMESTAMP,
      0, anno_extract, anno_outa, outb },
    { "ts_sec", B_4, T_TIMESTAMP_SEC,
      0, anno_extract, num_outa, outb },
    { "ts_usec", B_4, T_TIMESTAMP_USEC,
      0, anno_extract, num_outa, outb },
    { "ts_usec1", B_8, T_TIMESTAMP_USEC1,
      0, anno_extract, num_outa, outb },
    { "first_timestamp", B_8, T_FIRST_TIMESTAMP,
      0, anno_extract, anno_outa, utimestamp_outb },
    { "first_ntimestamp", B_8, T_FIRST_TIMESTAMP,
      0, anno_extract, anno_outa, outb },
    { "count", B_4, T_COUNT,
      0, anno_extract, num_outa, outb },
    { "link", B_1, T_LINK,
      0, anno_extract, num_outa, outb },
    { "paint", B_1, T_LINK,
      0, anno_extract, num_outa, outb },
    { "direction", B_1, T_DIRECTION,
      0, anno_extract, anno_outa, outb },
    { "aggregate", B_4, T_AGGREGATE,
      0, anno_extract, num_outa, outb },
    { "wire_len", B_4, T_WIRE_LEN,
      0, anno_extract, num_outa, outb }
};

static const IPSummaryDump::FieldReader anno_readers[] = {
    { "timestamp", B_8, T_TIMESTAMP, order_anno,
      anno_ina, utimestamp_inb, anno_inject },
    { "ntimestamp", B_8, T_TIMESTAMP, order_anno,
      anno_ina, inb, anno_inject },
    { "ts_sec", B_4, T_TIMESTAMP_SEC, order_anno,
      num_ina, inb, anno_inject },
    { "ts_usec", B_4, T_TIMESTAMP_USEC, order_anno,
      num_ina, inb, anno_inject },
    { "ts_usec1", B_8, T_TIMESTAMP_USEC1, order_anno,
      num_ina, inb, anno_inject },
    { "first_timestamp", B_8, T_FIRST_TIMESTAMP, order_anno,
      anno_ina, utimestamp_inb, anno_inject },
    { "first_ntimestamp", B_8, T_FIRST_TIMESTAMP, order_anno,
      anno_ina, inb, anno_inject },
    { "count", B_4, T_COUNT, order_anno,
      num_ina, inb, anno_inject },
    { "link", B_1, T_LINK, order_anno,
      anno_ina, inb, anno_inject },
    { "paint", B_1, T_LINK, order_anno,
      anno_ina, inb, anno_inject },
    { "direction", B_1, T_DIRECTION, order_anno,
      anno_ina, inb, anno_inject },
    { "aggregate", B_4, T_AGGREGATE, order_anno,
      num_ina, inb, anno_inject },
    { "wire_len", B_4, T_WIRE_LEN, order_anno,
      num_ina, inb, anno_inject }
};

static const IPSummaryDump::FieldSynonym anno_synonyms[] = {
    { "utimestamp", "timestamp" },
    { "ts", "utimestamp" },
    { "sec", "ts_sec" },
    { "usec", "ts_usec" },
    { "usec1", "ts_usec1" },
    { "first_utimestamp", "first_timestamp" },
    { "first_ts", "first_utimestamp" },
    { "pkt_count", "count" },
    { "packet_count", "count" },
    { "agg", "aggregate" },
    { "wire_length", "wire_len" }
};

}

void IPSummaryDump_Anno::static_initialize()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(anno_writers) / sizeof(anno_writers[0]); ++i)
	FieldWriter::add(&anno_writers[i]);
    for (size_t i = 0; i < sizeof(anno_readers) / sizeof(anno_readers[0]); ++i)
	FieldReader::add(&anno_readers[i]);
    for (size_t i = 0; i < sizeof(anno_synonyms) / sizeof(anno_synonyms[0]); ++i)
	FieldSynonym::add(&anno_synonyms[i]);
}

void IPSummaryDump_Anno::static_cleanup()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(anno_writers) / sizeof(anno_writers[0]); ++i)
	FieldWriter::remove(&anno_writers[i]);
    for (size_t i = 0; i < sizeof(anno_readers) / sizeof(anno_readers[0]); ++i)
	FieldReader::remove(&anno_readers[i]);
    for (size_t i = 0; i < sizeof(anno_synonyms) / sizeof(anno_synonyms[0]); ++i)
	FieldSynonym::remove(&anno_synonyms[i]);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_Anno)
CLICK_ENDDECLS
