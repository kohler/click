// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdumpinfo.{cc,hh} -- information used by IP summary dump elements
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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
#include <click/args.hh>
#include <click/ipflowid.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
CLICK_DECLS

static Vector<const void *> *writers;
static Vector<const void *> *readers;
static Vector<const void *> *synonyms;

static void field_add(Vector<const void *> *&vec, const void *value)
{
    if (!vec)
	vec = new Vector<const void *>;
    vec->push_back(value);
}

static void field_remove(Vector<const void *> *&vec, const void *value)
{
    if (vec) {
	const void **x = find(vec->begin(), vec->end(), value);
	if (x != vec->end())
	    vec->erase(x);
    }
}

static const void *field_find(Vector<const void *> *&vec, const String &name)
{
    if (vec)
	for (const void **x = vec->begin(); x != vec->end(); ++x) {
	    const IPSummaryDump::FieldSynonym *fs = reinterpret_cast<const IPSummaryDump::FieldSynonym *>(*x);
	    if (name == fs->name)
		return *x;
	}
    return 0;
}

void IPSummaryDumpInfo::static_cleanup()
{
    delete writers;
    delete readers;
    delete synonyms;
    writers = readers = synonyms = 0;
}



/////////////////////
// PARSING

namespace IPSummaryDump {

void FieldWriter::add(const FieldWriter *w)
{
    field_add(writers, w);
}

void FieldWriter::remove(const FieldWriter *w)
{
    field_remove(writers, w);
}

void FieldReader::add(const FieldReader *r)
{
    field_add(readers, r);
}

void FieldReader::remove(const FieldReader *r)
{
    field_remove(readers, r);
}

void FieldSynonym::add(const FieldSynonym *s)
{
    field_add(synonyms, s);
}

void FieldSynonym::remove(const FieldSynonym *s)
{
    field_remove(synonyms, s);
}


static String update_name(const String &name)
{
    // change spaces to underscores
    const char *s = find(name, ' ');
    if (s != name.end())
	return name.substring(name.begin(), s) + "_"
	    + name.substring(s + 1, name.end());

    // change "X" to "ip_X"
    if (find(name, '_') == name.end())
	return "ip_" + name;

    // not found
    return String();
}

static bool none_extract(PacketDesc &, const FieldWriter *)
{
    return false;
}

const FieldReader null_reader = {
    "none", B_0, 0, order_anno,
    num_ina, inb, 0
};

const FieldWriter null_writer = {
    "none", B_0, 0,
    0, none_extract, num_outa, outb
};

const FieldReader *FieldReader::find(const String &name)
{
    if (const void *x = field_find(synonyms, name)) {
	const FieldSynonym *s = reinterpret_cast<const FieldSynonym *>(x);
	return find(s->synonym);
    }
    if (const void *x = field_find(readers, name))
	return reinterpret_cast<const FieldReader *>(x);
    if (String new_name = update_name(name))
	return find(new_name);
    return 0;
}

const FieldWriter *FieldWriter::find(const String &name)
{
    if (const void *x = field_find(synonyms, name)) {
	const FieldSynonym *s = reinterpret_cast<const FieldSynonym *>(x);
	return find(s->synonym);
    }
    if (const void *x = field_find(writers, name))
	return reinterpret_cast<const FieldWriter *>(x);
    if (String new_name = update_name(name))
	return find(new_name);
    return 0;
}

static const char *field_missing_proto_name(int proto)
{
    switch (proto) {
      case IP_PROTO_TCP:
	return "TCP";
      case IP_PROTO_UDP:
	return "UDP";
      case IP_PROTO_ICMP:
	return "ICMP";
      case IP_PROTO_TCP_OR_UDP:
	return "TCP/UDP";
      default:
	return "?";
    }
}

bool hard_field_missing(const PacketDesc &d, int proto, int l)
{
    if (d.bad_sa && !*d.bad_sa) {
	if (proto == MISSING_ETHERNET)
	    *d.bad_sa << "!bad Ethernet header\n";
	else if (!d.iph)
	    *d.bad_sa << "!bad no IP header\n";
	else if (proto == MISSING_IP)
	    *d.bad_sa << "!bad truncated IP header capture\n";
	else if (d.network_length() > (int) offsetof(click_ip, ip_p)
		 && ((proto > MISSING_IP && proto < 256
		      && d.iph->ip_p != proto)
		     || (proto == IP_PROTO_TCP_OR_UDP
			 && d.iph->ip_p != IP_PROTO_TCP
			 && d.iph->ip_p != IP_PROTO_UDP)))
	    // wrong protocol is a silent error, not a bad packet
	    return false;
	else if (!IP_FIRSTFRAG(d.iph))
	    *d.bad_sa << "!bad fragmented " << field_missing_proto_name(proto) << " header\n";
	else if ((int) (d.transport_length() + EXTRA_LENGTH_ANNO(d.p)) >= l)
	    *d.bad_sa << "!bad truncated " << field_missing_proto_name(proto) << " header capture\n";
	else
	    *d.bad_sa << "!bad truncated " << field_missing_proto_name(proto) << " header\n";
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

void num_outa(const PacketDesc& d, const FieldWriter *f)
{
    if (f->type == B_8) {
#if HAVE_INT64_TYPES
	uint64_t v = ((uint64_t) d.u32[1] << 32) | d.u32[0];
	*d.sa << v;
#else
	// XXX silently truncate large numbers
	*d.sa << d.u32[0];
#endif
    } else
	*d.sa << d.v;
}

bool num_ina(PacketOdesc& d, const String &s, const FieldReader *f)
{
#if HAVE_INT64_TYPES
    if (f->type == B_8) {
	uint64_t v;
	if (!IntArg().parse(s, v))
	    return false;
	d.u32[0] = v;
	d.u32[1] = v >> 32;
	return true;
    }
#else
    // XXX die on large numbers
#endif
    if (!IntArg().parse(s, d.v))
	return false;
    if ((f->type == B_1 && d.v > 255) || (f->type == B_2 && d.v > 65535))
	return false;
    return true;
}

void outb(const PacketDesc& d, bool, const FieldWriter *f)
{
    switch (f->type) {
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
      case B_6PTR: {
	  char* c = d.sa->extend(6);
	  memcpy(c, d.vptr, 6);
	  break;
      }
      case B_8: {
	  char* c = d.sa->extend(8);
	  PUT4(c, d.u32[1]);
	  PUT4(c + 4, d.u32[0]);
	  break;
      }
      case B_4NET: {
	  char* c = d.sa->extend(4);
	  PUT4NET(c, d.v);
	  break;
      }
    }
}

const uint8_t *inb(PacketOdesc& d, const uint8_t *s, const uint8_t *end, const FieldReader *f)
{
    d.v = 0;
    switch (f->type) {
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
    case B_6PTR:
	if (s + 5 >= end)
	    goto bad;
	memcpy(d.u8, s, 6);
	return s + 6;
      case B_8:
	if (s + 7 >= end)
	    goto bad;
	d.u32[1] = GET4(s);
	d.u32[0] = GET4(s + 4);
	return s + 8;
      case B_4NET:
	if (s + 3 >= end)
	    goto bad;
	d.v = GET4NET(s);
	return s + 4;
      bad:
      default:
	d.clear_values();
	return end;
    }
}



void ip_prepare(PacketDesc &d, const FieldWriter *)
{
    Packet *p = const_cast<Packet *>(d.p);
    d.iph = (p->has_network_header() ? p->ip_header() : 0);
    d.tcph = (p->has_transport_header() ? p->tcp_header() : 0);
    d.udph = (p->has_transport_header() ? p->udp_header() : 0);
    d.icmph = (p->has_transport_header() ? p->icmp_header() : 0);

#define BAD(msg, hdr) do { if (d.bad_sa && !*d.bad_sa) *d.bad_sa << "!bad " << msg << '\n'; hdr = 0; } while (0)
#define BAD2(msg, val, hdr) do { if (d.bad_sa && !*d.bad_sa) *d.bad_sa << "!bad " << msg << val << '\n'; hdr = 0; } while (0)
    // check IP header
    if (!d.iph)
	/* nada */;
    else if (p->network_length() < (int) offsetof(click_ip, ip_id))
	BAD("truncated IP header", d.iph);
    else if (d.iph->ip_v != 4)
	BAD2("IP version ", d.iph->ip_v, d.iph);
    else if (d.iph->ip_hl < (sizeof(click_ip) >> 2))
	BAD2("IP header length ", d.iph->ip_hl, d.iph);
    else if (ntohs(d.iph->ip_len) < (d.iph->ip_hl << 2))
	BAD2("IP length ", ntohs(d.iph->ip_len), d.iph);
    else {
	// truncate packet length to IP length if necessary
	int ip_len = ntohs(d.iph->ip_len);
	if (p->network_length() > ip_len) {
	    SET_EXTRA_LENGTH_ANNO(p, EXTRA_LENGTH_ANNO(p) + p->network_length() - ip_len);
	    d.tailpad = p->network_length() - ip_len;
	} else if (d.careful_trunc && p->network_length() + EXTRA_LENGTH_ANNO(p) < (uint32_t) ip_len) {
	    /* This doesn't actually kill the IP header. */
	    int scratch;
	    BAD2("truncated IP missing ", (ntohs(d.iph->ip_len) - p->network_length() - EXTRA_LENGTH_ANNO(p)), scratch);
	    (void) scratch;
	}
    }

    // check TCP header
    if (!d.iph || !d.tcph
	|| d.network_length() <= (uint32_t)(d.iph->ip_hl << 2)
	|| d.iph->ip_p != IP_PROTO_TCP
	|| !IP_FIRSTFRAG(d.iph))
	d.tcph = 0;
    else if (d.transport_length() > 12
	     && d.tcph->th_off < (sizeof(click_tcp) >> 2))
	BAD2("TCP header length ", d.tcph->th_off, d.tcph);

    // check UDP header
    if (!d.iph || !d.udph
	|| d.network_length() - d.tailpad <= (uint32_t)(d.iph->ip_hl << 2)
	|| d.iph->ip_p != IP_PROTO_UDP
	|| !IP_FIRSTFRAG(d.iph))
	d.udph = 0;

    // check ICMP header
    if (!d.iph || !d.icmph
	|| d.network_length() <= (uint32_t)(d.iph->ip_hl << 2)
	|| d.iph->ip_p != IP_PROTO_ICMP
	|| !IP_FIRSTFRAG(d.iph))
	d.icmph = 0;
#undef BAD
#undef BAD2

    // Adjust extra length, since we calculate lengths here based on ip_len.
    if (d.iph && EXTRA_LENGTH_ANNO(p) > 0) {
	uint32_t full_len = d.network_length() + EXTRA_LENGTH_ANNO(p);
	if (d.iph->ip_len != 0xFFFF || full_len <= 0xFFFF)
	    SET_EXTRA_LENGTH_ANNO(p, 0);
	else
	    SET_EXTRA_LENGTH_ANNO(p, full_len - 0xFFFF);
    }
}


bool PacketOdesc::hard_make_ip()
{
    if (!is_ip)
	return false;
    if (!p->has_network_header())
	p->set_network_header(p->data(), 0);
    if (p->network_length() < (int) sizeof(click_ip)) {
	if (!(p = p->put(sizeof(click_ip) - p->network_length())))
	    return false;
	p->set_network_header(p->network_header(), sizeof(click_ip));
	click_ip *iph = p->ip_header();
	iph->ip_v = 4;
	iph->ip_hl = sizeof(click_ip) >> 2;
	iph->ip_p = default_ip_p;
	iph->ip_len = 0;
	iph->ip_off = 0;
	iph->ip_ttl = 100;
	if (default_ip_flowid) {
	    iph->ip_src.s_addr = default_ip_flowid->saddr().addr();
	    iph->ip_dst.s_addr = default_ip_flowid->daddr().addr();
	}
    }
    return true;
}

bool PacketOdesc::hard_make_transp()
{
    click_ip *iph = p->ip_header();
    if (IP_FIRSTFRAG(iph)) {
	int len;
	switch (iph->ip_p) {
	case IP_PROTO_TCP:
	    len = sizeof(click_tcp);
	    break;
	case IP_PROTO_UDP:
	case IP_PROTO_UDPLITE:
	    len = sizeof(click_udp);
	    break;
	case IP_PROTO_DCCP:
	    len = 12;
	    break;
	case IP_PROTO_ICMP:
	    len = sizeof(click_icmp);
	    break;
	default:
	    return true;
	}
	if (want_len > 0
	    && want_len < (uint32_t) p->transport_header_offset() + len)
	    len = want_len - p->transport_header_offset();

	if (p->transport_length() < len) {
	    int xlen = (len < 4 ? 4 : len);
	    if (!(p = p->put(xlen - p->transport_length())))
		return false;
	    if (p->ip_header()->ip_p == IP_PROTO_TCP && len >= 13)
		p->tcp_header()->th_off = sizeof(click_tcp) >> 2;
	    if (default_ip_flowid) {
		click_udp *udph = p->udp_header();
		udph->uh_sport = default_ip_flowid->sport();
		udph->uh_dport = default_ip_flowid->dport();
	    }
	    if (xlen > len)
		p->take(xlen - len);
	}
    }

    return true;
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
