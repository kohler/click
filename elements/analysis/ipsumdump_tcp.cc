// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_tcp.{cc,hh} -- IP transport summary dump unparsers
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

#include "ipsumdump_tcp.hh"
#include <click/packet.hh>
#include <click/nameinfo.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <click/confparse.hh>
CLICK_DECLS

namespace IPSummaryDump {

enum { T_TCP_SEQ, T_TCP_ACK, T_TCP_FLAGS, T_TCP_WINDOW, T_TCP_URP, T_TCP_OPT,
       T_TCP_NTOPT, T_TCP_SACK, T_TCP_OFF };

static bool tcp_extract(PacketDesc& d, const FieldWriter *f)
{
    int transport_length = d.transport_length();
    switch (f->user_data) {

#define CHECK(l) do { if (!d.tcph || transport_length < (l)) return field_missing(d, IP_PROTO_TCP, (l)); } while (0)

      case T_TCP_SEQ:
	CHECK(8);
	d.v = ntohl(d.tcph->th_seq);
	return true;
      case T_TCP_ACK:
	CHECK(12);
	d.v = ntohl(d.tcph->th_ack);
	return true;
      case T_TCP_FLAGS:
	CHECK(14);
	d.v = d.tcph->th_flags | (d.tcph->th_flags2 << 8);
	return true;
      case T_TCP_OFF:
	CHECK(13);
	d.v = d.tcph->th_off << 2;
	return true;
      case T_TCP_WINDOW:
	CHECK(16);
	d.v = ntohs(d.tcph->th_win);
	return true;
      case T_TCP_URP:
	CHECK(20);
	d.v = ntohs(d.tcph->th_urp);
	return true;
      case T_TCP_OPT:
	// need to check that d.tcph->th_off exists
	if (!d.tcph || transport_length < 13 || (d.tcph->th_off > 5 && transport_length < (int)(d.tcph->th_off << 2)))
	    goto no_tcp_opt;
	if (d.tcph->th_off <= 5)
	    d.vptr[0] = d.vptr[1] = 0;
	else {
	    d.vptr[0] = (const uint8_t *) (d.tcph + 1);
	    d.vptr[1] = d.vptr[0] + (d.tcph->th_off << 2) - sizeof(click_tcp);
	}
	return true;
      case T_TCP_NTOPT:
      case T_TCP_SACK:
	// need to check that d.tcph->th_off exists
	if (!d.tcph || transport_length < 13)
	    goto no_tcp_opt;
	else if (d.tcph->th_off <= 5
		 || (d.tcph->th_off == 8 && transport_length >= 24
		     && *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
	    d.vptr[0] = d.vptr[1] = 0;
	else if (transport_length < (int)(d.tcph->th_off << 2))
	    goto no_tcp_opt;
	else {
	    d.vptr[0] = (const uint8_t *) (d.tcph + 1);
	    d.vptr[1] = d.vptr[0] + (d.tcph->th_off << 2) - sizeof(click_tcp);
	}
	return true;

#undef CHECK

      default:
	return false;
      no_tcp_opt:
	return field_missing(d, IP_PROTO_TCP, transport_length + 1);
    }
}

static void tcp_inject(PacketOdesc& d, const FieldReader *f)
{
    if (!d.make_ip(IP_PROTO_TCP) || !d.make_transp())
	return;

    click_tcp *tcph = d.p->tcp_header();
    switch (f->user_data) {
    case T_TCP_SEQ:
	tcph->th_seq = htonl(d.v);
	break;
    case T_TCP_ACK:
	tcph->th_ack = htonl(d.v);
	break;
    case T_TCP_FLAGS:
	tcph->th_flags = d.v;
	tcph->th_flags2 = d.v >> 8;
	break;
    case T_TCP_OFF:
	d.v = (d.v + 3) & ~3;
	if ((int) d.v > (int) (tcph->th_off << 2)) {
	    int more = d.v - (tcph->th_off << 2);
	    if (!(d.p = d.p->put(more)))
		return;
	    tcph = d.p->tcp_header();
	    memset(d.p->transport_header() + d.v - more, TCPOPT_EOL, more);
	}
	tcph->th_off = d.v >> 2;
	break;
    case T_TCP_WINDOW:
	tcph->th_win = htons(d.v);
	break;
    case T_TCP_URP:
	tcph->th_urp = htons(d.v);
	break;
    case T_TCP_OPT:
    case T_TCP_NTOPT:
    case T_TCP_SACK: {
        d.have_tcp_hl = true;
	if (!d.vptr[0] || d.vptr[0] == d.vptr[1])
	    return;
	int olen = d.vptr[1] - d.vptr[0];
	int th_off = (sizeof(click_tcp) + olen + 3) & ~3;
	if (d.p->transport_length() < th_off) {
	    if (!(d.p = d.p->put(th_off - d.p->transport_length())))
		return;
	    tcph = d.p->tcp_header();
	}
	if (th_off > (int) (tcph->th_off << 2))
	    tcph->th_off = th_off >> 2;
	memcpy(d.p->transport_header() + sizeof(click_tcp), d.vptr[0], olen);
	memset(d.p->transport_header() + sizeof(click_tcp) + olen,
	       TCPOPT_EOL, th_off - olen);
	break;
    }
    }
}

static void tcp_outa(const PacketDesc& d, const FieldWriter *f)
{
    switch (f->user_data) {
      case T_TCP_FLAGS:
	if (d.v == (TH_ACK | TH_PUSH))
	    *d.sa << 'P' << 'A';
	else if (d.v == TH_ACK)
	    *d.sa << 'A';
	else if (d.v == 0)
	    *d.sa << '.';
	else
	    for (int flag = 0; flag < 9; flag++)
		if (d.v & (1 << flag))
		    *d.sa << tcp_flags_word[flag];
	break;
      case T_TCP_OPT:
	if (!d.vptr[0] || d.vptr[0] == d.vptr[1])
	    *d.sa << '.';
	else
	    unparse_tcp_opt(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_TCPOPT_ALL_NOPAD);
	break;
      case T_TCP_NTOPT:
	if (!d.vptr[0] || d.vptr[0] == d.vptr[1])
	    *d.sa << '.';
	else
	    unparse_tcp_opt(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_TCPOPT_NTALL);
	break;
      case T_TCP_SACK:
	if (!d.vptr[0] || d.vptr[0] == d.vptr[1])
	    *d.sa << '.';
	else
	    unparse_tcp_opt(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_TCPOPT_SACK);
	break;
    }
}

static bool tcp_ina(PacketOdesc& d, const String &str, const FieldReader *f)
{
    switch (f->user_data) {
    case T_TCP_FLAGS:
	if (str.equals(".", 1))
	    d.v = 0;
	else if (!str)
	    return false;
	else if (isdigit((unsigned char) str[0]))
	    return IntArg().parse(str, d.v) && d.v < 0x1000;
	else {
	    d.v = 0;
	    for (const char *s = str.begin(); s != str.end(); s++)
		if (uint8_t fm = IPSummaryDump::tcp_flag_mapping[(unsigned char) *s])
		    d.v |= 1 << (fm - 1);
		else
		    return false;
	}
	return true;
    default:
	return false;
    }
}

static void tcp_outb(const PacketDesc& d, bool ok, const FieldWriter *f)
{
    switch (f->user_data) {
      case T_TCP_OPT:
	if (!ok || !d.vptr[0] || d.vptr[0] == d.vptr[1])
	    *d.sa << '\0';
	else
	    unparse_tcp_opt_binary(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_TCPOPT_ALL);
	break;
      case T_TCP_NTOPT:
	if (!ok || !d.vptr[0] || d.vptr[0] == d.vptr[1])
	    *d.sa << '\0';
	else
	    unparse_tcp_opt_binary(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_TCPOPT_NTALL);
	break;
      case T_TCP_SACK:
	if (!ok || !d.vptr[0] || d.vptr[0] == d.vptr[1])
	    *d.sa << '\0';
	else
	    unparse_tcp_opt_binary(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_TCPOPT_SACK);
	break;
    }
}

static const uint8_t* tcp_inb(PacketOdesc& d, const uint8_t* s, const uint8_t* ends, const FieldReader *f)
{
    switch (f->user_data) {
      case T_TCP_OPT:
      case T_TCP_NTOPT:
      case T_TCP_SACK:
	if (s + s[0] + 1 <= ends) {
	    d.vptr[0] = s + 1;
	    d.vptr[1] = d.vptr[0] + s[0];
	    return s + s[0] + 1;
	}
	break;
    }
    return ends;
}


static int tcp_opt_mask_mapping[] = {
    DO_TCPOPT_PADDING, DO_TCPOPT_PADDING,	// EOL, NOP
    DO_TCPOPT_MSS, DO_TCPOPT_WSCALE,		// MAXSEG, WSCALE
    DO_TCPOPT_SACK, DO_TCPOPT_SACK,		// SACK_PERMITTED, SACK
    DO_TCPOPT_UNKNOWN, DO_TCPOPT_UNKNOWN,	// 6, 7
    DO_TCPOPT_TIMESTAMP				// TIMESTAMP
};

void unparse_tcp_opt(StringAccum& sa, const uint8_t* opt, int opt_len, int mask)
{
    int initial_sa_len = sa.length();
    const uint8_t *end_opt = opt + opt_len;
    const char *sep = "";

    while (opt < end_opt)
	switch (*opt) {
	  case TCPOPT_EOL:
	    if (mask & DO_TCPOPT_PADDING)
		sa << sep << "eol";
	    goto done;
	  case TCPOPT_NOP:
	    if (mask & DO_TCPOPT_PADDING) {
		sa << sep << "nop";
		sep = ";";
	    }
	    opt++;
	    break;
	  case TCPOPT_MAXSEG:
	    if (opt + opt[1] > end_opt || opt[1] != TCPOLEN_MAXSEG)
		goto bad_opt;
	    if (!(mask & DO_TCPOPT_MSS))
		goto unknown;
	    sa << sep << "mss" << ((opt[2] << 8) | opt[3]);
	    opt += TCPOLEN_MAXSEG;
	    sep = ";";
	    break;
	  case TCPOPT_WSCALE:
	    if (opt + opt[1] > end_opt || opt[1] != TCPOLEN_WSCALE)
		goto bad_opt;
	    if (!(mask & DO_TCPOPT_WSCALE))
		goto unknown;
	    sa << sep << "wscale" << (int)(opt[2]);
	    opt += TCPOLEN_WSCALE;
	    sep = ";";
	    break;
	  case TCPOPT_SACK_PERMITTED:
	    if (opt + opt[1] > end_opt || opt[1] != TCPOLEN_SACK_PERMITTED)
		goto bad_opt;
	    if (!(mask & DO_TCPOPT_SACK))
		goto unknown;
	    sa << sep << "sackok";
	    opt += TCPOLEN_SACK_PERMITTED;
	    sep = ";";
	    break;
	  case TCPOPT_SACK: {
	      if (opt + opt[1] > end_opt || (opt[1] % 8 != 2))
		  goto bad_opt;
	      if (!(mask & DO_TCPOPT_SACK))
		  goto unknown;
	      const uint8_t *end_sack = opt + opt[1];
	      for (opt += 2; opt < end_sack; opt += 8) {
		  uint32_t buf[2];
		  memcpy(&buf[0], opt, 8);
		  sa << sep << "sack" << ntohl(buf[0]) << '-' << ntohl(buf[1]);
		  sep = ";";
	      }
	      break;
	  }
	  case TCPOPT_TIMESTAMP: {
	      if (opt + opt[1] > end_opt || opt[1] != TCPOLEN_TIMESTAMP)
		  goto bad_opt;
	      if (!(mask & DO_TCPOPT_TIMESTAMP))
		  goto unknown;
	      uint32_t buf[2];
	      memcpy(&buf[0], opt + 2, 8);
	      sa << sep << "ts" << ntohl(buf[0]) << ':' << ntohl(buf[1]);
	      opt += TCPOLEN_TIMESTAMP;
	      sep = ";";
	      break;
	  }
	  default: {
	      if (opt + opt[1] > end_opt || opt[1] < 2)
		  goto bad_opt;
	      if (!(mask & DO_TCPOPT_UNKNOWN))
		  goto unknown;
	      sa << sep << (int)(opt[0]);
	      const uint8_t *end_this_opt = opt + opt[1];
	      char opt_sep = '=';
	      for (opt += 2; opt < end_this_opt; opt++) {
		  sa << opt_sep << (int)(*opt);
		  opt_sep = ':';
	      }
	      sep = ";";
	      break;
	  }
	  unknown:
	    opt += (opt[1] >= 2 ? opt[1] : 128);
	    break;
	}

  done:
    if (sa.length() == initial_sa_len)
	sa << '.';
    return;

  bad_opt:
    sa.set_length(initial_sa_len);
    sa << '?';
}

void unparse_tcp_opt(StringAccum& sa, const click_tcp* tcph, int mask)
{
    unparse_tcp_opt(sa, reinterpret_cast<const uint8_t *>(tcph + 1), (tcph->th_off << 2) - sizeof(click_tcp), mask);
}

void unparse_tcp_opt_binary(StringAccum& sa, const uint8_t* opt, int opt_len, int mask)
{
    if (mask == (int)DO_TCPOPT_ALL) {
	// store all options
	sa.append((char)opt_len);
	sa.append(opt, opt_len);
    }

    const uint8_t *end_opt = opt + opt_len;
    int initial_sa_len = sa.length();
    sa.append('\0');

    while (opt < end_opt) {
	// one-byte options
	if (*opt == TCPOPT_EOL) {
	    if (mask & DO_TCPOPT_PADDING)
		sa.append(opt, 1);
	    goto done;
	} else if (*opt == TCPOPT_NOP) {
	    if (mask & DO_TCPOPT_PADDING)
		sa.append(opt, 1);
	    opt++;
	    continue;
	}

	// quit copying options if you encounter something obviously invalid
	if (opt[1] < 2 || opt + opt[1] > end_opt)
	    break;

	int this_content = (*opt > TCPOPT_TIMESTAMP ? (int)DO_TCPOPT_UNKNOWN : tcp_opt_mask_mapping[*opt]);
	if (mask & this_content)
	    sa.append(opt, opt[1]);
	opt += opt[1];
    }

  done:
    sa[initial_sa_len] = sa.length() - initial_sa_len - 1;
}

void unparse_tcp_opt_binary(StringAccum& sa, const click_tcp *tcph, int mask)
{
    unparse_tcp_opt_binary(sa, reinterpret_cast<const uint8_t *>(tcph + 1), (tcph->th_off << 2) - sizeof(click_tcp), mask);
}

static void append_net_uint32_t(StringAccum &sa, uint32_t u)
{
    sa << (char)(u >> 24) << (char)(u >> 16) << (char)(u >> 8) << (char)u;
}

static bool tcp_opt_ina(PacketOdesc &d, const String &str, const FieldReader *f)
{
    if (!str || str.equals(".", 1))
	return true;
    else if (str.equals("-", 1))
	return false;
    const uint8_t *s = reinterpret_cast<const uint8_t *>(str.begin());
    const uint8_t *end = reinterpret_cast<const uint8_t *>(str.end());
    int contents = DO_TCPOPT_ALL;
    if (f->user_data == T_TCP_SACK)
	contents = DO_TCPOPT_SACK;
    else if (f->user_data == T_TCP_NTOPT)
	contents = DO_TCPOPT_NTALL;
    d.sa.clear();

    while (1) {
	uint32_t u1, u2;

	if (s + 3 < end && memcmp(s, "mss", 3) == 0
	    && (contents & DO_TCPOPT_MSS)) {
	    u1 = 0x10000U;	// bad value
	    s = cp_integer(s + 3, end, 0, &u1);
	    if (u1 <= 0xFFFFU)
		d.sa << (char)TCPOPT_MAXSEG << (char)TCPOLEN_MAXSEG << (char)(u1 >> 8) << (char)u1;
	    else
		goto bad_opt;
	} else if (s + 6 < end && memcmp(s, "wscale", 6) == 0
		   && (contents & DO_TCPOPT_WSCALE)) {
	    u1 = 256;		// bad value
	    s = cp_integer(s + 6, end, 0, &u1);
	    if (u1 <= 255)
		d.sa << (char)TCPOPT_WSCALE << (char)TCPOLEN_WSCALE << (char)u1;
	    else
		goto bad_opt;
	} else if (s + 6 <= end && memcmp(s, "sackok", 6) == 0
		   && (contents & DO_TCPOPT_SACK)) {
	    d.sa << (char)TCPOPT_SACK_PERMITTED << (char)TCPOLEN_SACK_PERMITTED;
	    s += 6;
	} else if (s + 4 < end && memcmp(s, "sack", 4) == 0
		   && (contents & DO_TCPOPT_SACK)) {
	    // combine adjacent SACK options into a block
	    int sa_pos = d.sa.length();
	    d.sa << (char)TCPOPT_SACK << (char)0;
	    s += 4;
	    while (1) {
		const unsigned char *t = cp_integer(s, end, 0, &u1);
		if (t >= end || (*t != ':' && *t != '-'))
		    goto bad_opt;
		t = cp_integer(t + 1, end, 0, &u2);
		append_net_uint32_t(d.sa, u1);
		append_net_uint32_t(d.sa, u2);
		if (t < s + 3) // at least 1 digit in each block
		    goto bad_opt;
		s = t;
		if (s + 5 >= end || memcmp(s, ",sack", 5) != 0)
		    break;
		s += 5;
	    }
	    d.sa[sa_pos + 1] = (char)(d.sa.length() - sa_pos);
	} else if (s + 2 < end && memcmp(s, "ts", 2) == 0
		   && (contents & DO_TCPOPT_TIMESTAMP)) {
	    const unsigned char *t = cp_integer(s + 2, end, 0, &u1);
	    if (t >= end || *t != ':')
		goto bad_opt;
	    t = cp_integer(t + 1, end, 0, &u2);
	    if (d.sa.length() == 0)
		d.sa << (char)TCPOPT_NOP << (char)TCPOPT_NOP;
	    d.sa << (char)TCPOPT_TIMESTAMP << (char)TCPOLEN_TIMESTAMP;
	    append_net_uint32_t(d.sa, u1);
	    append_net_uint32_t(d.sa, u2);
	    if (t < s + 5)	// at least 1 digit in each block
		goto bad_opt;
	    s = t;
	} else if (s < end && isdigit(*s)
		   && (contents & DO_TCPOPT_UNKNOWN)) {
	    s = cp_integer(s, end, 0, &u1);
	    if (u1 >= 256)
		goto bad_opt;
	    d.sa << (char)u1;
	    if (s + 1 < end && *s == '=' && isdigit(s[1])) {
		int pos0 = d.sa.length();
		d.sa << (char)0;
		do {
		    s = cp_integer(s + 1, end, 0, &u1);
		    if (u1 >= 256)
			goto bad_opt;
		    d.sa << (char)u1;
		} while (s + 1 < end && *s == ':' && isdigit(s[1]));
		if (d.sa.length() > pos0 + 254)
		    goto bad_opt;
		d.sa[pos0] = (char)(d.sa.length() - pos0 + 1);
	    }
	} else if (s + 3 <= end && memcmp(s, "nop", 3) == 0
		   && (contents & DO_TCPOPT_PADDING)) {
	    d.sa << (char)TCPOPT_NOP;
	    s += 3;
	} else if (s + 3 <= end && strncmp((const char *) s, "eol", 3) == 0
		   && (contents & DO_TCPOPT_PADDING)
		   && (s + 3 == end || s[3] != ',')) {
	    d.sa << (char)TCPOPT_EOL;
	    s += 3;
	} else
	    goto bad_opt;

	if (s >= end || isspace(*s)) {
	    // check for improper padding
	    while (d.sa.length() > 40 && d.sa[0] == TCPOPT_NOP) {
		memmove(&d.sa[0], &d.sa[1], d.sa.length() - 1);
		d.sa.pop_back();
	    }
	    // options too long?
	    if (d.sa.length() > 40)
		goto bad_opt;
	    // otherwise ok
	    d.vptr[0] = reinterpret_cast<const uint8_t *>(d.sa.begin());
	    d.vptr[1] = reinterpret_cast<const uint8_t *>(d.sa.end());
	    return true;
	} else if (*s != ',' && *s != ';')
	    goto bad_opt;

	s++;
    }

  bad_opt:
    return false;
}

static const FieldWriter tcp_writers[] = {
    { "tcp_seq", B_4, T_TCP_SEQ,
      ip_prepare, tcp_extract, num_outa, outb },
    { "tcp_ack", B_4, T_TCP_ACK,
      ip_prepare, tcp_extract, num_outa, outb },
    { "tcp_off", B_1, T_TCP_OFF,
      ip_prepare, tcp_extract, num_outa, outb },
    { "tcp_flags", B_1, T_TCP_FLAGS,
      ip_prepare, tcp_extract, tcp_outa, outb },
    { "tcp_window", B_2, T_TCP_WINDOW,
      ip_prepare, tcp_extract, num_outa, outb },
    { "tcp_urp", B_2, T_TCP_URP,
      ip_prepare, tcp_extract, num_outa, outb },
    { "tcp_opt", B_SPECIAL, T_TCP_OPT,
      ip_prepare, tcp_extract, tcp_outa, tcp_outb },
    { "tcp_ntopt", B_SPECIAL, T_TCP_NTOPT,
      ip_prepare, tcp_extract, tcp_outa, tcp_outb },
    { "tcp_sack", B_SPECIAL, T_TCP_SACK,
      ip_prepare, tcp_extract, tcp_outa, tcp_outb }
};

static const FieldReader tcp_readers[] = {
    { "tcp_seq", B_4, T_TCP_SEQ, order_transp,
      num_ina, inb, tcp_inject },
    { "tcp_ack", B_4, T_TCP_ACK, order_transp,
      num_ina, inb, tcp_inject },
    { "tcp_off", B_1, T_TCP_OFF, order_transp - 1,
      num_ina, inb, tcp_inject },
    { "tcp_flags", B_1, T_TCP_FLAGS, order_transp,
      tcp_ina, inb, tcp_inject },
    { "tcp_window", B_2, T_TCP_WINDOW, order_transp,
      num_ina, inb, tcp_inject },
    { "tcp_urp", B_2, T_TCP_URP, order_transp,
      num_ina, inb, tcp_inject },
    { "tcp_opt", B_SPECIAL, T_TCP_OPT, order_transp + 3,
      tcp_opt_ina, tcp_inb, tcp_inject },
    { "tcp_ntopt", B_SPECIAL, T_TCP_NTOPT, order_transp + 2,
      tcp_opt_ina, tcp_inb, tcp_inject },
    { "tcp_sack", B_SPECIAL, T_TCP_SACK, order_transp + 1,
      tcp_opt_ina, tcp_inb, tcp_inject }
};

static const FieldSynonym tcp_synonyms[] = {
    { "tcp_seqno", "tcp_seq" },
    { "tcp_ackno", "tcp_ack" },
    { "tcp_win", "tcp_window" }
};

}

void IPSummaryDump_TCP::static_initialize()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(tcp_writers) / sizeof(tcp_writers[0]); ++i)
	FieldWriter::add(&tcp_writers[i]);
    for (size_t i = 0; i < sizeof(tcp_readers) / sizeof(tcp_readers[0]); ++i)
	FieldReader::add(&tcp_readers[i]);
    for (size_t i = 0; i < sizeof(tcp_synonyms) / sizeof(tcp_synonyms[0]); ++i)
	FieldSynonym::add(&tcp_synonyms[i]);
}

void IPSummaryDump_TCP::static_cleanup()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(tcp_writers) / sizeof(tcp_writers[0]); ++i)
	FieldWriter::remove(&tcp_writers[i]);
    for (size_t i = 0; i < sizeof(tcp_readers) / sizeof(tcp_readers[0]); ++i)
	FieldReader::remove(&tcp_readers[i]);
    for (size_t i = 0; i < sizeof(tcp_synonyms) / sizeof(tcp_synonyms[0]); ++i)
	FieldSynonym::remove(&tcp_synonyms[i]);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_TCP)
CLICK_ENDDECLS
