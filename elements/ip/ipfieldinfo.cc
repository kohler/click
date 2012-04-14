/*
 * ipfieldinfo.{cc,hh} -- IP-packet filter with tcpdumplike syntax
 * Eddie Kohler
 *
 * Copyright (c) 2005 Regents of the University of California
 * Copyright (c) 2012 Eddie Kohler
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
#include "ipfieldinfo.hh"
#include <click/integers.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#if CLICK_BSDMODULE
# include <machine/stdarg.h>
#else
# include <stdarg.h>
#endif
CLICK_DECLS

#define BITFIELD(proto, offset, length)			\
	(((proto) << IPField::PROTO_SHIFT)		\
	| ((offset) << IPField::OFFSET_SHIFT)		\
	| (((length) - 1) << IPField::LENGTH_SHIFT)	\
	| IPField::MARKER)

static const StaticNameDB::Entry ip_fields[] = {
    { "df",	BITFIELD(0, 6*8 + 1, 1) },
    { "dport",	BITFIELD(IP_PROTO_TCP_OR_UDP, 2*8, 16) },
    { "dscp",	BITFIELD(0, 1*8, 6) },
    { "dst",	BITFIELD(0, 16*8, 32) },
    { "ecn",	BITFIELD(0, 1*8 + 6, 2) },
    { "fragoff",BITFIELD(0, 6*8 + 3, 13) },
    { "hl",	BITFIELD(0, 4, 4) },
    { "id",	BITFIELD(0, 4*8, 16) },
    { "len",	BITFIELD(0, 2*8, 16) },
    { "mf",	BITFIELD(0, 6*8 + 2, 1) },
    { "off",	BITFIELD(0, 6*8, 16) },
    { "proto",	BITFIELD(0, 9*8, 8) },
    { "rf",	BITFIELD(0, 6*8, 1) },
    { "sport",	BITFIELD(IP_PROTO_TCP_OR_UDP, 0, 16) },
    { "src",	BITFIELD(0, 12*8, 32) },
    { "sum",	BITFIELD(0, 10*8, 16) },
    { "tos",	BITFIELD(0, 1*8, 8) },
    { "ttl",	BITFIELD(0, 8*8, 8) },
    { "vers",	BITFIELD(0, 0, 4) }
};

static const StaticNameDB::Entry udp_fields[] = {
    { "dport",	BITFIELD(IP_PROTO_UDP, 2*8, 16) },
    { "len",	BITFIELD(IP_PROTO_UDP, 4*8, 16) },
    { "sport",	BITFIELD(IP_PROTO_UDP, 0, 16) },
    { "sum",	BITFIELD(IP_PROTO_UDP, 6*8, 16) }
};

static const StaticNameDB::Entry tcp_fields[] = {
    { "ack",	BITFIELD(IP_PROTO_TCP, 13*8+3, 1) },
    { "ackno",	BITFIELD(IP_PROTO_TCP, 8*8, 32) },
    { "dport",	BITFIELD(IP_PROTO_TCP, 2*8, 16) },
    { "fin",	BITFIELD(IP_PROTO_TCP, 13*8+7, 1) },
    { "flags",	BITFIELD(IP_PROTO_TCP, 13*8, 8) },
    { "hl",	BITFIELD(IP_PROTO_TCP, 12*8, 4) },
    { "psh",	BITFIELD(IP_PROTO_TCP, 13*8+4, 1) },
    { "push",	BITFIELD(IP_PROTO_TCP, 13*8+4, 1) },
    { "rst",	BITFIELD(IP_PROTO_TCP, 13*8+5, 1) },
    { "seq",	BITFIELD(IP_PROTO_TCP, 4*8, 32) },
    { "seqno",	BITFIELD(IP_PROTO_TCP, 4*8, 32) },
    { "sport",	BITFIELD(IP_PROTO_TCP, 0, 16) },
    { "sum",	BITFIELD(IP_PROTO_TCP, 16*8, 16) },
    { "syn",	BITFIELD(IP_PROTO_TCP, 13*8+6, 1) },
    { "urg",	BITFIELD(IP_PROTO_TCP, 13*8+2, 1) },
    { "urp",	BITFIELD(IP_PROTO_TCP, 18*8, 16) },
    { "win",	BITFIELD(IP_PROTO_TCP, 14*8, 16) }
};

static const StaticNameDB::Entry icmp_fields[] = {
    { "code",	BITFIELD(IP_PROTO_ICMP, 1*8, 8) },
    { "sum",	BITFIELD(IP_PROTO_ICMP, 2*8, 16) },
    { "type",	BITFIELD(IP_PROTO_ICMP, 0*8, 8) }
};

static const StaticNameDB::Entry tcp_or_udp_fields[] = {
    { "dport",	BITFIELD(IP_PROTO_TCP_OR_UDP, 2*8, 16) },
    { "sport",	BITFIELD(IP_PROTO_TCP_OR_UDP, 0, 16) }
};


IPField::IPField(int proto, int bit_offset, int bit_length)
{
    if (proto >= 0 && proto <= MAX_PROTO && bit_offset >= 0 && bit_length >= 0) {
	if (bit_offset <= MAX_OFFSET && bit_length <= MAX_LENGTH + 1)
	    _val = (proto << PROTO_SHIFT) | (bit_offset << OFFSET_SHIFT) | ((bit_length - 1) << LENGTH_SHIFT) | MARKER;
	else if ((bit_offset & 7) == 0 && (bit_length & 7) == 0 && bit_length <= (MAX_LENGTH + 1) << 3)
	    _val = (proto << PROTO_SHIFT) | BYTES | ((bit_offset >> 3) << OFFSET_SHIFT) | (((bit_length >> 3) - 1) << LENGTH_SHIFT) | MARKER;
	else
	    _val = -1;
    } else
	_val = -1;
}

static NameDB *dbs[5];

void
IPFieldInfo::static_initialize()
{
    dbs[0] = new StaticNameDB(NameInfo::T_IP_FIELDNAME, String(), ip_fields, sizeof(ip_fields) / sizeof(ip_fields[0]));
    dbs[1] = new StaticNameDB(NameInfo::T_ICMP_FIELDNAME, String(), icmp_fields, sizeof(icmp_fields) / sizeof(icmp_fields[0]));
    dbs[2] = new StaticNameDB(NameInfo::T_TCP_FIELDNAME, String(), tcp_fields, sizeof(tcp_fields) / sizeof(tcp_fields[0]));
    dbs[3] = new StaticNameDB(NameInfo::T_UDP_FIELDNAME, String(), udp_fields, sizeof(udp_fields) / sizeof(udp_fields[0]));
    dbs[4] = new StaticNameDB(NameInfo::T_IP_FIELDNAME + IP_PROTO_TCP_OR_UDP, String(), tcp_or_udp_fields, sizeof(tcp_or_udp_fields) / sizeof(tcp_or_udp_fields[0]));
    for (int i = 0; i < 5; i++)
	if (dbs[i])
	    NameInfo::installdb(dbs[i], 0);
}

void
IPFieldInfo::static_cleanup()
{
    for (int i = 0; i < 5; i++) {
	delete dbs[i];
	dbs[i] = 0;
    }
}

const char *
cp_scanf(const char *begin, const char *end, const char *format, ...)
{
    const char *s, *ss;
    bool must_space = false;
    uint32_t delimiters[8];
    bool know_delimiters = false;

    va_list val;
    va_start(val, format);

    for (s = begin; *format; format++)
	if (*format == ' ') {
	    if (must_space && format[1] != '%' && (s == end || !isspace((unsigned char) *s)))
		goto kill;
	    while (s < end && isspace((unsigned char) *s))
		s++;
	    must_space = false;
	} else if (*format == '%') {
	    format++;
	    switch (*format) {
	      case 'u': {
		  uint32_t *d = va_arg(val, uint32_t *);
		  if ((ss = cp_integer(s, end, 0, d)) == s)
		      goto kill;
		  s = ss;
		  must_space = false;
		  break;
	      }
	      case 'N': {
		  uint32_t nametype = va_arg(val, uint32_t);
		  Element *elt = va_arg(val, Element *);
		  const char *w = s;
		  while (s < end && !isspace((unsigned char) *s) && (!know_delimiters || !(delimiters[((unsigned char) *s) >> 5] & (1 << (((unsigned char) *s) % 32)))))
		      s++;
		  uint32_t *store = va_arg(val, uint32_t *);
		  if (w == s || !NameInfo::query(nametype, elt, String(w, s), store, sizeof(*store)))
		      goto kill;
		  must_space = false;
		  break;
	      }
	      case 'D': {
		  const char *delim = va_arg(val, const char *);
		  if (!know_delimiters)
		      memset(delimiters, 0, sizeof(delimiters));
		  for (; *delim; delim++)
		      delimiters[((unsigned char) *delim) >> 5] |= (1 << (((unsigned char) *delim) % 32));
		  know_delimiters = true;
		  break;
	      }
	      case 'B':
		if (s != end && (isalnum((unsigned char) *s) || *s == '_'))
		    goto kill;
		break;
	      case '%':
		goto normal;
	    }
	} else {
	  normal:
	    if (s == end || *s != *format)
		goto kill;
	    s++;
	    must_space = true;
	}

    va_end(val);
    return s;

  kill:
    va_end(val);
    return 0;
}

static const char * const cp_ip_field_messages[] = {
    "expected 'HEADER [NAME] [{OFFSET:LENGTH}] [/PREFIX] [& MASK]'",
    "bad offset or length in TCP/IP field",
    "bad prefix or mask in TCP/IP field"
};

static const char *
cp_ip_field_helper(const char *begin, int which, ErrorHandler *errh)
{
    if (errh)
	errh->error(cp_ip_field_messages[which]);
    return begin;
}

const char *
IPField::parse(const char *begin, const char *end, int proto, IPField *result, ErrorHandler *errh, Element *elt)
{
    // determine header, if any
    int32_t header = -1;
    const char *ehdr;
    if ((ehdr = cp_scanf(begin, end, "ip proto %u", &header)))
	/* OK */;
    else if ((ehdr = cp_scanf(begin, end, "ip%B")))
	header = 0;
    else if ((ehdr = cp_scanf(begin, end, "%N", NameInfo::T_IP_PROTO, elt, &header)))
	/* OK */;
    else if ((header = proto) < 0)
	return cp_ip_field_helper(begin, 0, errh);

    // field name
    IPField field(-1);
    const char *enam;
    if ((enam = cp_scanf(ehdr, end, " %D%N", "/[{&", NameInfo::T_IP_FIELDNAME + header, elt, &field)))
	/* OK */;
    else
	enam = ehdr;

    // limitation
    int32_t offset = -1, length = -1;
    const char *elim;
    if ((elim = cp_scanf(enam, end, " [ %u ]", &offset)))
	offset *= 8, length = 8;
    else if ((elim = cp_scanf(enam, end, " [ %u : %u ]", &offset, &length)))
	offset *= 8, length *= 8;
    else if ((elim = cp_scanf(enam, end, " [ %u - %u ]", &offset, &length)))
	offset *= 8, length = (length - offset + 1) * 8;
    else if ((elim = cp_scanf(enam, end, " { %u }", &offset)))
	length = 1;
    else if ((elim = cp_scanf(enam, end, " { %u : %u }", &offset, &length)))
	/* OK */;
    else if ((elim = cp_scanf(enam, end, " { %u - %u }", &offset, &length)))
	length = length - offset + 1;
    else if (!field.ok()) {
	click_chatter("%.*s", end - enam, enam);
	return cp_ip_field_helper(begin, 0, errh);}
    else
	elim = enam;
    if (offset >= 0 && length <= 0)
	return cp_ip_field_helper(begin, 1, errh);
    if (field.ok() && (offset >= field.bit_length() || offset + length > field.bit_length()))
	return cp_ip_field_helper(begin, 1, errh);
    else if (field.ok() && offset >= 0)
	field = IPField(field.proto(), field.bit_offset() + offset, length);
    else if (offset >= 0)
	field = IPField(header, offset, length);
    else if (!field.ok())
	return cp_ip_field_helper(begin, 1, errh);

    // limitations
    const char *epfx, *emask;
    if ((epfx = cp_scanf(elim, end, " / %u", &length))) {
	if (length > field.bit_length())
	    return cp_ip_field_helper(begin, 2, errh);
	field = IPField(field.proto(), field.bit_offset(), length);
    } else
	epfx = elim;
    if ((emask = cp_scanf(epfx, end, " & %u", &length))) {
	offset = ffs_lsb((uint32_t) length) - 1;
	int msb = ffs_lsb((uint32_t) length + (1 << offset)) - 1;
	if (length == 0 || ((length + (1 << offset)) & (length + (1 << offset) - 1)) != 0 || msb > field.bit_length())
	    return cp_ip_field_helper(begin, 2, errh);
	field = IPField(field.proto(), field.bit_offset() + field.bit_length() - msb, msb - offset);
    } else
	emask = epfx;

    *result = field;
    return emask;
}

String
IPField::unparse(Element *elt, bool tcpdump_rules)
{
    if (!ok())
	return String::make_stable("<bad>");

    StringAccum sa;
    String s;
    int32_t val = proto();
    if (val == 0)
	sa << "ip";
    else if ((s = NameInfo::revquery(NameInfo::T_IP_PROTO, elt, &val, 4)))
	sa << s;
    else
	sa << "ip proto " << proto();

    if ((s = NameInfo::revquery(NameInfo::T_IP_FIELDNAME + proto(), elt, &_val, 4))) {
	sa << ' ' << s;
	return sa.take_string();
    }

    int bo = bit_offset(), bl = bit_length();

    for (int container = 8; container < 64 && !tcpdump_rules; container *= 2)
	if (bo / container == (bo + bl - 1) / container) {
	    IPField x(proto(), bo & ~(container - 1), container);
	    if ((s = NameInfo::revquery(NameInfo::T_IP_FIELDNAME + proto(), elt, &x, 4))) {
		sa << ' ' << s;
		bo &= container - 1;
		if (bo == 0) {
		    sa << '/' << bl;
		    return sa.take_string();
		}
		break;
	    }
	}

    uint32_t maskval = 0;
    if (tcpdump_rules && (bo % 8 != 0 || bl % 8 != 0)
	&& bl / 32 == (bo + bl - 1) / 32) {
	maskval = ((1 << bl) - 1) << (7 - (bo + bl - 1) % 8);
	bl = (bo % 8 + bl + 7) & ~7;
	bo &= ~7;
    }

    if (bo % 8 == 0 && bl == 8)
	sa << '[' << (bo / 8) << ']';
    else if (bo % 8 == 0 && bl % 8 == 0)
	sa << '[' << (bo / 8) << ':' << (bl / 8) << ']';
    else if (bl == 1)
	sa << '{' << bo << '}';
    else
	sa << '{' << bo << ':' << bl << '}';
    if (maskval)
	sa << " & " << maskval;
    return sa.take_string();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPFieldInfo)
