// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdumpinfo.{cc,hh} -- information used by IP summary dump elements
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
#include <clicknet/ip.h>
CLICK_DECLS

int
IPSummaryDumpInfo::parse_content(const String &word)
{
    if (word == "timestamp" || word == "ts")
	return W_TIMESTAMP;
    else if (word == "sec" || word == "ts_sec")
	return W_TIMESTAMP_SEC;
    else if (word == "usec" || word == "ts_usec")
	return W_TIMESTAMP_USEC;
    else if (word == "usec1" || word == "ts_usec1")
	return W_TIMESTAMP_USEC1;
    else if (word == "src" || word == "ip_src")
	return W_IP_SRC;
    else if (word == "dst" || word == "ip_dst")
	return W_IP_DST;
    else if (word == "sport")
	return W_SPORT;
    else if (word == "dport")
	return W_DPORT;
    else if (word == "frag" || word == "ip_frag")
	return W_IP_FRAG;
    else if (word == "fragoff" || word == "ip_fragoff")
	return W_IP_FRAGOFF;
    else if (word == "len" || word == "length" || word == "ip_len")
	return W_IP_LEN;
    else if (word == "id" || word == "ip_id")
	return W_IP_ID;
    else if (word == "proto" || word == "ip_proto" || word == "ip_p")
	return W_IP_PROTO;
    else if (word == "tcp_seq" || word == "tcp_seqno")
	return W_TCP_SEQ;
    else if (word == "tcp_ack" || word == "tcp_ackno")
	return W_TCP_ACK;
    else if (word == "tcp_flags")
	return W_TCP_FLAGS;
    else if (word == "tcp_sack")
	return W_TCP_SACK;
    else if (word == "tcp_opt")
	return W_TCP_OPT;
    else if (word == "tcp_ntopt")
	return W_TCP_NTOPT;
    else if (word == "payload_len" || word == "payload_length")
	return W_PAYLOAD_LEN;
    else if (word == "count" || word == "pkt_count" || word == "packet_count")
	return W_COUNT;
    else if (word == "payload")
	return W_PAYLOAD;
    else if (word == "payload_md5")
	return W_PAYLOAD_MD5;
    else if (word == "link" || word == "direction")
	return W_LINK;
    else if (word == "aggregate" || word == "agg")
	return W_AGGREGATE;
    else if (word == "first_timestamp" || word == "first_ts")
	return W_FIRST_TIMESTAMP;
    else if (word == "ntimestamp")
	return W_NTIMESTAMP;
    else if (word == "first_ntimestamp")
	return W_FIRST_NTIMESTAMP;
    else if (word == "tcp_window" || word == "tcp_win")
	return W_TCP_WINDOW;
    else if (word == "ip_opt")
	return W_IP_OPT;
    else if (word == "ip_tos")
	return W_IP_TOS;
    else if (word == "ip_ttl")
	return W_IP_TTL;
    else if (word == "ip_capture_len")
	return W_IP_CAPTURE_LEN;
    else if (word == "none")
	return W_NONE;
    else if (word == "tcp_urp")
	return W_TCP_URP;
    else if (find(word, ' ') != word.end()) {
	const char *space = find(word, ' ');
	return parse_content(word.substring(word.begin(), space) + "_" + word.substring(space + 1, word.end()));
    } else
	return -1;
}

static int content_binary_sizes[] = {
    0, 8, 4, 4, 4,	// W_NONE, W_TIMESTAMP, W_TS_SEC, W_TS_USEC, W_IP_SRC
    4, 4, 1, 2, 2,	// W_IP_DST, W_IP_LEN, W_IP_PROTO, W_IP_ID, W_SPORT
    2, 4, 4, 1, 4,	// W_DPORT, W_TCP_SEQ, W_TCP_ACK, W_TCP_FLAGS,
			// W_PAYLOAD_LEN
    4, 1, 2, -10000, 1,	// W_COUNT, W_IP_FRAG, W_IP_FRAGOFF, W_PAYLOAD, W_LINK
    4, 4, 4, 4, 8,      // W_AGGREGATE, W_TCP_SACK, W_TCP_OPT, W_TCP_NTOPT,
			// W_FIRST_TIMESTAMP
    2, 4, 1, 1, 8,	// W_TCP_WINDOW, W_IP_OPT, W_IP_TOS, W_IP_TTL,
			// W_TIMESTAMP_USEC1
    4, 2, 8, 8, 16	// W_IP_CAPTURE_LEN, W_TCP_URP, W_NTIMESTAMP,
			// W_FIRST_NTIMESTAMP, W_PAYLOAD_MD5
};

int
IPSummaryDumpInfo::content_binary_size(int content)
{
    if (content < 0 || content >= (int)(sizeof(content_binary_sizes) / sizeof(content_binary_sizes[0])))
	return -10000;
    else
	return content_binary_sizes[content];
}



/////////////////////
// PARSING

namespace IPSummaryDump {

static bool none_extract(PacketDesc&, int)
{
    return false;
}

static Field* fields;
const Field null_field = {
    "none", B_0, 0, none_extract, num_outa, outb, inb, 0, 0
};

int Field::binary_size() const
{
    switch (thunk & B_TYPEMASK) {
      case B_0:		return 0;
      case B_1:		return 1;
      case B_2:		return 2;
      case B_4:		return 4;
      case B_8:		return 8;
      case B_16:	return 16;
      case B_4NET:	return 4;
      case B_SPECIAL:	return 4;
      default:		return -1;
    }
}
    
const Field* find_field(const String& name, bool likely_synonyms)
{
    // search for "name"
    for (Field* f = fields; f; f = f->next)
	if (name == f->name)
	    return (f->synonym ? f->synonym : f);
    if (name == "none")
	return &null_field;
    if (!likely_synonyms)
	return 0;
    
    // if not found, change spaces to underscores and try again
    const char *s = find(name, ' ');
    if (s != name.end())
	return find_field(name.substring(name.begin(), s) + "_" + name.substring(s + 1, name.end()));
    
    // if not found, change "X" to "ip_X" and try again
    if (find(name, '_') == name.end())
	return find_field("ip_" + name);
    
    // not found
    return 0;
}

int register_unparser(const char* name, int thunk,
		      void (*prepare)(PacketDesc&),
		      bool (*extract)(PacketDesc&, int),
		      void (*outa)(const PacketDesc&, int),
		      void (*outb)(const PacketDesc&, bool, int),
		      const uint8_t *(*inb)(PacketDesc&, const uint8_t*, const uint8_t*, int))
{
    Field* f = const_cast<Field*>(find_field(name, false));
    if (f) {
	if (f == &null_field
	    || f->synonym
	    || f->thunk != thunk
	    || (f->prepare && f->prepare != prepare)
	    || (f->extract && f->extract != extract)
	    || (f->outa && f->outa != outa)
	    || (f->outb && f->outb != outb)
	    || (f->inb && f->inb != inb))
	    return -1;
	if (!f->prepare && prepare)
	    f->prepare = prepare;
	if (!f->extract && extract)
	    f->extract = extract;
	if (!f->outa && outa)
	    f->outa = outa;
	if (!f->outb && outb)
	    f->outb = outb;
	if (!f->inb && inb)
	    f->inb = inb;
    } else if (!f) {
	if (!(f = new Field))
	    return -1;
	f->name = name;
	f->thunk = thunk;
	f->prepare = prepare;
	f->extract = extract;
	f->outa = outa;
	f->outb = outb;
	f->inb = inb;
	f->synonym = 0;
	f->next = fields;
	fields = f;
    }
    return 0;
}

void static_cleanup()
{
    while (Field* f = fields) {
	fields = f->next;
	delete f;
    }
}

int register_synonym(const char* name, const char* synonym)
{
    Field* synf = const_cast<Field*>(find_field(synonym));
    if (!synf)
	return -1;
    
    Field* f = const_cast<Field*>(find_field(name, false));
    if (f)
	return f->synonym == synf;
    else {
	if (!(f = new Field))
	    return -1;
	f->name = name;
	f->synonym = synf;
	f->next = fields;
	fields = f;
	return 0;
    }
}

bool field_missing(const PacketDesc& d, int what, const char* header_name, int l)
{
    if (d.bad_sa && !*d.bad_sa) {
	*d.bad_sa << "!bad ";
	if (what == MISSING_IP || what == MISSING_IP_TRANSPORT) {
	    if (!d.iph)
		*d.bad_sa << "no IP header";
	    else if (what == MISSING_IP)
		*d.bad_sa << "truncated IP header capture";
	    else if ((int) (d.p->transport_length() + EXTRA_LENGTH_ANNO(d.p)) >= l)
		*d.bad_sa << "truncated " << header_name << " header capture";
	    else if (IP_ISFRAG(d.iph))
		*d.bad_sa << "fragmented " << header_name << " header";
	    else
		*d.bad_sa << "truncated " << header_name << " header";
	} else
	    *d.bad_sa << header_name << " header";
	*d.bad_sa << '\n';
    }
    return false;
}

#ifdef i386
# define PUT4NET(p, d)	*reinterpret_cast<uint32_t *>((p)) = (d)
# define PUT4(p, d)	*reinterpret_cast<uint32_t *>((p)) = htonl((d))
# define PUT2(p, d)	*reinterpret_cast<uint16_t *>((p)) = htons((d))
# define GET4NET(p)	*reinterpret_cast<const uint32_t *>((p))
# define GET4(p)	ntohl(*reinterpret_cast<const uint32_t *>((p)))
# define GET2(p)	ntohs(*reinterpret_cast<const uint16_t *>((p)))
#else
# define PUT4NET(p, d)	do { uint32_t d__ = ntohl((d)); (p)[0] = d__>>24; (p)[1] = d__>>16; (p)[2] = d__>>8; (p)[3] = d__; } while (0)
# define PUT4(p, d)	do { (p)[0] = (d)>>24; (p)[1] = (d)>>16; (p)[2] = (d)>>8; (p)[3] = (d); } while (0)
# define PUT2(p, d)	do { (p)[0] = (d)>>8; (p)[1] = (d); } while (0)
# define GET4NET(p)	htonl((p)[0]<<24 | (p)[1]<<16 | (p)[2]<<8 | (p)[3])
# define GET4(p)	((p)[0]<<24 | (p)[1]<<16 | (p)[2]<<8 | (p)[3])
# define GET2(p)	((p)[0]<<8 | (p)[1])
#endif
#define PUT1(p, d)	((p)[0] = (d))

void num_outa(const PacketDesc& d, int)
{
    *d.sa << d.v;
}

void outb(const PacketDesc& d, bool, int thunk)
{
    switch (thunk & B_TYPEMASK) {
      case B_0:
	break;
      case B_1: {
	  char* c = d.sa->extend(1);
	  PUT1(c, d.v);
	  break;
      }
      case B_2: {
	  char* c = d.sa->extend(2);
	  PUT2(c, d.v);
	  break;
      }
      case B_4: {
	  char* c = d.sa->extend(4);
	  PUT4(c, d.v);
	  break;
      }
      case B_8: {
	  char* c = d.sa->extend(8);
	  PUT4(c, d.v);
	  PUT4(c + 4, d.v2);
	  break;
      }
      case B_4NET: {
	  char* c = d.sa->extend(4);
	  PUT4NET(c, d.v);
	  break;
      }
    }
}

const uint8_t *inb(PacketDesc& d, const uint8_t *s, const uint8_t *end, int thunk)
{
    d.v = 0;
    switch (thunk & B_TYPEMASK) {
      case B_0:
	return s;
      case B_1:
	if (s >= end)
	    goto bad;
	d.v = s[0];
	return s + 1;
      case B_2:
	if (s + 1 >= end)
	    goto bad;
	d.v = GET2(s);
	return s + 2;
      case B_4:
	if (s + 3 >= end)
	    goto bad;
	d.v = GET4(s);
	return s + 4;
      case B_8:
	if (s + 7 >= end)
	    goto bad;
	d.v = GET4(s);
	d.v2 = GET4(s + 4);
	return s + 8;
      case B_4NET:
	if (s + 3 >= end)
	    goto bad;
	d.v = GET4NET(s);
	return s + 4;
      bad:
      default:
	d.v = d.v2 = 0;
	return end;
    }
}


const char tcp_flags_word[] = "FSRPAUECN";

const uint8_t tcp_flag_mapping[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x0-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x1-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x2-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x3-
    0, 5, 0, 8, 0, 7, 1, 0, 0, 0, 0, 0, 0, 0, 9, 0, // 0x4-
    4, 0, 3, 2, 0, 6, 0, 8, 7, 8, 0, 0, 0, 0, 0, 0, // 0x5-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x6-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x7-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x8-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x9-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xA-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xB-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xC-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xD-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xE-
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 0xF-    
};

}

ELEMENT_REQUIRES(userlevel)
ELEMENT_PROVIDES(IPSummaryDumpInfo)
ELEMENT_PROVIDES(IPSummaryDump)
CLICK_ENDDECLS
