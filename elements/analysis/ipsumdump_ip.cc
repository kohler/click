// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_ip.{cc,hh} -- IP network layer IP summary dump unparsers
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2008 Regents of the University of California
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

#include "ipsumdump_ip.hh"
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/md5.h>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/args.hh>
#include <click/ipflowid.hh>
CLICK_DECLS

enum { T_IP_SRC, T_IP_DST, T_IP_TOS, T_IP_TTL, T_IP_FRAG, T_IP_FRAGOFF,
       T_IP_ID, T_IP_SUM, T_IP_PROTO, T_IP_OPT, T_IP_LEN, T_IP_CAPTURE_LEN,
       T_SPORT, T_DPORT, T_IP_HL, T_IP_DSCP, T_IP_ECN };

namespace IPSummaryDump {

static bool ip_extract(PacketDesc& d, const FieldWriter *f)
{
    uint32_t network_length = d.network_length();
    switch (f->user_data) {

	// IP header properties
#define CHECK(l) do { if (!d.iph || network_length < (l)) return field_missing(d, MISSING_IP, (l)); } while (0)
      case T_IP_SRC:
	CHECK(16);
	d.v = d.iph->ip_src.s_addr;
	return true;
      case T_IP_DST:
	CHECK(20);
	d.v = d.iph->ip_dst.s_addr;
	return true;
      case T_IP_TOS:
	CHECK(2);
	d.v = d.iph->ip_tos;
	return true;
      case T_IP_DSCP:
	CHECK(2);
	d.v = d.iph->ip_tos >> 2;
	return true;
      case T_IP_ECN:
	CHECK(2);
	d.v = d.iph->ip_tos & IP_ECNMASK;
	return true;
      case T_IP_TTL:
	CHECK(9);
	d.v = d.iph->ip_ttl;
	return true;
      case T_IP_FRAG:
	CHECK(8);
	if (IP_ISFRAG(d.iph))
	    d.v = (IP_FIRSTFRAG(d.iph) ? 'F' : 'f');
	else
	    d.v = (d.iph->ip_off & htons(IP_DF) ? '!' : '.');
	return true;
      case T_IP_FRAGOFF:
	CHECK(8);
	d.v = ntohs(d.iph->ip_off);
	return true;
      case T_IP_ID:
	CHECK(6);
	d.v = ntohs(d.iph->ip_id);
	return true;
      case T_IP_SUM:
	CHECK(12);
	d.v = ntohs(d.iph->ip_sum);
	return true;
      case T_IP_PROTO:
	CHECK(10);
	d.v = d.iph->ip_p;
	return true;
      case T_IP_OPT:
	if (!d.iph || (d.iph->ip_hl > 5 && network_length < (uint32_t)(d.iph->ip_hl << 2)))
	    return field_missing(d, MISSING_IP, (d.iph ? d.iph->ip_hl << 2 : 20));
	if (d.iph->ip_hl <= 5)
	    d.vptr[0] = d.vptr[1] = 0;
	else {
	    d.vptr[0] = (const uint8_t *) (d.iph + 1);
	    d.vptr[1] = d.vptr[0] + (d.iph->ip_hl << 2) - sizeof(click_ip);
	}
	return true;
      case T_IP_LEN:
	if (d.iph)
	    d.v = ntohs(d.iph->ip_len);
	else
	    d.v = d.length();
	if (d.force_extra_length)
	    d.v += EXTRA_LENGTH_ANNO(d.p);
	return true;
      case T_IP_HL:
	CHECK(1);
	d.v = d.iph->ip_hl << 2;
	return true;
      case T_IP_CAPTURE_LEN: {
	  uint32_t allow_len = (d.iph ? network_length : d.length());
	  uint32_t len = (d.iph ? ntohs(d.iph->ip_len) : allow_len);
	  d.v = (len < allow_len ? len : allow_len);
	  return true;
      }
#undef CHECK

      default:
	return false;
    }
}

static inline bool ip_proto_has_udp_ports(int ip_p)
{
    return ip_p == IP_PROTO_TCP || ip_p == IP_PROTO_UDP
	|| ip_p == IP_PROTO_DCCP || ip_p == IP_PROTO_UDPLITE;
}

static void ip_inject(PacketOdesc& d, const FieldReader *f)
{
    if (!d.make_ip(0))
	return;

    click_ip *iph = d.p->ip_header();
    switch (f->user_data) {
	// IP header properties
    case T_IP_SRC:
	iph->ip_src.s_addr = d.v;
	break;
    case T_IP_DST:
	iph->ip_dst.s_addr = d.v;
	break;
    case T_IP_TOS:
	iph->ip_tos = d.v;
	break;
    case T_IP_DSCP:
	iph->ip_tos = (iph->ip_tos & IP_ECNMASK) | ((d.v << 2) & IP_DSCPMASK);
	break;
    case T_IP_ECN:
	iph->ip_tos = (iph->ip_tos & IP_DSCPMASK) | (d.v & IP_ECNMASK);
	break;
    case T_IP_TTL:
	iph->ip_ttl = d.v;
	break;
    case T_IP_FRAG:
    case T_IP_FRAGOFF:
	iph->ip_off = htons(d.v);
	break;
    case T_IP_ID:
	iph->ip_id = htons(d.v);
	break;
    case T_IP_SUM:
	iph->ip_sum = htons(d.v);
	break;
    case T_IP_PROTO:
	iph->ip_p = d.v;
	break;
    case T_IP_OPT: {
        d.have_ip_hl = true;
	if (!d.vptr[0])
	    return;
	int olen = d.vptr[1] - d.vptr[0];
	int ip_hl = (sizeof(click_ip) + olen + 3) & ~3;
	if (d.p->network_length() < ip_hl) {
	    if (!(d.p = d.p->put(ip_hl - d.p->network_length())))
		return;
	    iph = d.p->ip_header();
	}
	if (ip_hl > (int) (iph->ip_hl << 2)) {
	    d.p->set_ip_header(iph, ip_hl);
	    iph->ip_hl = ip_hl >> 2;
	}
	memcpy(d.p->network_header() + sizeof(click_ip), d.vptr[0], olen);
	memset(d.p->network_header() + sizeof(click_ip) + olen,
	       IPOPT_EOL, ip_hl - olen);
	break;
    }
    case T_IP_HL:
        d.have_ip_hl = true;
	d.v = (d.v + 3) & ~3;
	if ((int) d.v > (int) (iph->ip_hl << 2)) {
	    int more = d.v - (iph->ip_hl << 2);
	    if (!(d.p = d.p->put(more)))
		return;
	    iph = d.p->ip_header();
	    d.p->set_ip_header(iph, d.v);
	    memset(d.p->transport_header() - more, IPOPT_EOL, more);
	}
	iph->ip_hl = d.v >> 2;
	break;
    case T_IP_LEN:
	d.want_len = d.p->network_header_offset() + d.v;
	break;
    case T_IP_CAPTURE_LEN:
        /* Best to simply ignore capture length on input. */
	break;
    }
}

static void ip_outa(const PacketDesc& d, const FieldWriter *f)
{
    switch (f->user_data) {
      case T_IP_SRC:
      case T_IP_DST:
	*d.sa << IPAddress(d.v);
	break;
      case T_IP_FRAG:
	*d.sa << (char) d.v;
	break;
      case T_IP_FRAGOFF:
	*d.sa << ((d.v & IP_OFFMASK) << 3);
	if (d.v & IP_MF)
	    *d.sa << '+';
	if (d.v & IP_DF)
	    *d.sa << '!';
	break;
      case T_IP_PROTO:
	switch (d.v) {
	  case IP_PROTO_TCP:	*d.sa << 'T'; break;
	  case IP_PROTO_UDP:	*d.sa << 'U'; break;
	  case IP_PROTO_ICMP:	*d.sa << 'I'; break;
	  default:		*d.sa << d.v; break;
	}
	break;
    case T_IP_ECN:
	switch (d.v) {
	case IP_ECN_NOT_ECT:	*d.sa << "no"; break;
	case IP_ECN_ECT1:	*d.sa << "ect1"; break;
	case IP_ECN_ECT2:	*d.sa << "ect2"; break;
	case IP_ECN_CE:		*d.sa << "ce"; break;
	}
	break;
      case T_IP_OPT:
	if (!d.vptr[0])
	    *d.sa << '.';
	else
	    unparse_ip_opt(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_IPOPT_ALL_NOPAD);
	break;
    }
}

static bool ip_ina(PacketOdesc& d, const String &s, const FieldReader *f)
{
    switch (f->user_data) {
    case T_IP_SRC:
    case T_IP_DST: {
	IPAddress a;
	if (IPAddressArg().parse(s, a, d.e)) {
	    d.v = a.addr();
	    return true;
	}
	break;
    }
    case T_IP_FRAG:
    case T_IP_FRAGOFF: {
	if (s.length() == 1) {
	    if (s[0] == '.') {
		d.v = 0;
		return true;
	    } else if (s[0] == '!') {
		d.v = IP_DF;
		return true;
	    } else if (s[0] == 'F') {
		d.v = IP_MF;
		return true;
	    } else if (s[0] == 'f') {
		d.v = 100;	// arbitrary nonzero offset
		return true;
	    }
	}
	d.v = 0;
	const char *new_end = cp_integer(s.begin(), s.end(), 0, &d.v);
	if (d.minor_version > 0)
	    d.v >>= 3;
	for (; new_end != s.end(); ++new_end)
	    if (*new_end == '!')
		d.v |= IP_DF;
	    else if (*new_end == '+')
		d.v |= IP_MF;
	    else
		break;
	if (new_end == s.end() && s.length())
	    return true;
	break;
    }
    case T_IP_PROTO:
	if (s.equals("T", 1)) {
	    d.v = IP_PROTO_TCP;
	    return true;
	} else if (s.equals("U", 1)) {
	    d.v = IP_PROTO_UDP;
	    return true;
	} else if (s.equals("I", 1)) {
	    d.v = IP_PROTO_ICMP;
	    return true;
	} else if (IntArg().parse(s, d.v) && d.v < 256)
	    return true;
	break;
    case T_IP_ECN:
	if (s.length() == 1 && s[0] >= '0' && s[0] <= '3') {
	    d.v = s[0] - '0';
	    return true;
	} else if (s.equals("no", 2) || s.equals("-", 1)) {
	    d.v = IP_ECN_NOT_ECT;
	    return true;
	} else if (s.equals("ect1", 4) || s.equals("ECT(1)", 6)) {
	    d.v = IP_ECN_ECT1;
	    return true;
	} else if (s.equals("ect2", 4) || s.equals("ECT(0)", 6)) {
	    d.v = IP_ECN_ECT2;
	    return true;
	} else if (s.equals("ce", 2) || s.equals("CE", 2)) {
	    d.v = IP_ECN_CE;
	    return true;
	}
	break;
#if 0
      case T_IP_OPT:
	if (!d.vptr[0])
	    *d.sa << '.';
	else
	    unparse_ip_opt(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_IPOPT_ALL_NOPAD);
	break;
#endif
    }
    return false;
}

static void ip_outb(const PacketDesc& d, bool ok, const FieldWriter *f)
{
    if (f->user_data == T_IP_OPT) {
	if (!ok || !d.vptr[0])
	    *d.sa << '\0';
	else
	    unparse_ip_opt_binary(*d.sa, d.vptr[0], d.vptr[1] - d.vptr[0], DO_IPOPT_ALL);
    }
}

static const uint8_t* ip_inb(PacketOdesc& d, const uint8_t *s, const uint8_t *ends, const FieldReader *f)
{
    if (f->user_data == T_IP_OPT && s + s[0] + 1 <= ends) {
	d.vptr[0] = s + 1;
	d.vptr[1] = d.vptr[0] + s[0];
	return s + s[0] + 1;
    } else
	return ends;
}


static bool transport_extract(PacketDesc& d, const FieldWriter *f)
{
    const Packet *p = d.p;
    switch (f->user_data) {
	// TCP/UDP header properties
    case T_SPORT:
    case T_DPORT: {
	bool dport = (f->user_data == T_DPORT);
	if (d.iph
	    && d.network_length() > (uint32_t) (d.iph->ip_hl << 2)
	    && IP_FIRSTFRAG(d.iph)
	    && ip_proto_has_udp_ports(d.iph->ip_p)
	    && d.transport_length() >= (dport ? 4 : 2)) {
	    const click_udp *udph = p->udp_header();
	    d.v = ntohs(dport ? udph->uh_dport : udph->uh_sport);
	    return true;
	}
	return field_missing(d, IP_PROTO_TCP_OR_UDP, dport ? 4 : 2);
    }
    }
    return false;
}

static void transport_inject(PacketOdesc& d, const FieldReader *f)
{
    if (!d.make_ip(0) || !d.make_transp())
	return;
    click_ip *iph = d.p->ip_header();
    if (iph->ip_p && !ip_proto_has_udp_ports(iph->ip_p))
	return;

    switch (f->user_data) {
	// TCP/UDP header properties
    case T_SPORT:
	d.p->udp_header()->uh_sport = htons(d.v);
	break;
    case T_DPORT:
	d.p->udp_header()->uh_dport = htons(d.v);
	break;
    }
}


#define U DO_IPOPT_UNKNOWN
static int ip_opt_mask_mapping[] = {
    DO_IPOPT_PADDING, DO_IPOPT_PADDING,		// EOL, NOP
    U, U, U, U,	U,				// 2, 3, 4, 5, 6
    DO_IPOPT_ROUTE,				// RR
    U, U, U, U, U, U, U, U, U, U, U, U, U,	// 8-20
    U, U, U, U, U, U, U, U, U, U,		// 21-30
    U, U, U, U, U, U, U, U, U, U,		// 31-40
    U, U, U, U, U, U, U, U, U, U,		// 41-50
    U, U, U, U, U, U, U, U, U, U,		// 51-60
    U, U, U, U, U, U, U,			// 61-67
    DO_IPOPT_TS, U, U,				// TS, 69-70
    U, U, U, U, U, U, U, U, U, U,		// 71-80
    U, U, U, U, U, U, U, U, U, U,		// 81-90
    U, U, U, U, U, U, U, U, U, U,		// 91-100
    U, U, U, U, U, U, U, U, U, U,		// 101-110
    U, U, U, U, U, U, U, U, U, U,		// 111-120
    U, U, U, U, U, U, U, U, U,			// 121-129
    DO_IPOPT_UNKNOWN, DO_IPOPT_ROUTE,		// SECURITY, LSRR
    U, U, U, U,					// 132-135
    DO_IPOPT_UNKNOWN, DO_IPOPT_ROUTE,		// SATID, SSRR
    U, U, U,					// 138-140
    U, U, U, U, U, U, U,			// 141-147
    DO_IPOPT_UNKNOWN				// RA
};
#undef U

void unparse_ip_opt(StringAccum& sa, const uint8_t* opt, int opt_len, int mask)
{
    int initial_sa_len = sa.length();
    const uint8_t *end_opt = opt + opt_len;
    const char *sep = "";

    while (opt < end_opt)
	switch (*opt) {
	  case IPOPT_EOL:
	    if (mask & DO_IPOPT_PADDING)
		sa << sep << "eol";
	    goto done;
	  case IPOPT_NOP:
	    if (mask & DO_IPOPT_PADDING) {
		sa << sep << "nop";
		sep = ";";
	    }
	    opt++;
	    break;
	  case IPOPT_RR:
	    if (opt + opt[1] > end_opt || opt[1] < 3 || opt[2] < 4)
		goto bad_opt;
	    if (!(mask & DO_IPOPT_ROUTE))
		goto unknown;
	    sa << sep << "rr";
	    goto print_route;
	  case IPOPT_LSRR:
	    if (opt + opt[1] > end_opt || opt[1] < 3 || opt[2] < 4)
		goto bad_opt;
	    if (!(mask & DO_IPOPT_ROUTE))
		goto unknown;
	    sa << sep << "lsrr";
	    goto print_route;
	  case IPOPT_SSRR:
	    if (opt + opt[1] > end_opt || opt[1] < 3 || opt[2] < 4)
		goto bad_opt;
	    if (!(mask & DO_IPOPT_ROUTE))
		goto unknown;
	    sa << sep << "ssrr";
	    goto print_route;
	  print_route: {
		const uint8_t *o = opt + 3, *ol = opt + opt[1], *op = opt + opt[2] - 1;
		sep = "";
		sa << '{';
		for (; o + 4 <= ol; o += 4) {
		    if (o == op) {
			if (opt[0] == IPOPT_RR)
			    break;
			sep = "^";
		    }
		    sa << sep << (int)o[0] << '.' << (int)o[1] << '.' << (int)o[2] << '.' << (int)o[3];
		    sep = ",";
		}
		if (o == ol && o == op && opt[0] != IPOPT_RR)
		    sa << '^';
		sa << '}';
		if (o + 4 <= ol && o == op && opt[0] == IPOPT_RR)
		    sa << '+' << (ol - o) / 4;
		opt = ol;
		sep = ";";
		break;
	    }
	  case IPOPT_TS: {
	      if (opt + opt[1] > end_opt || opt[1] < 4 || opt[2] < 5)
		  goto bad_opt;
	      const uint8_t *o = opt + 4, *ol = opt + opt[1], *op = opt + opt[2] - 1;
	      int flag = opt[3] & 0xF;
	      int size = (flag >= 1 && flag <= 3 ? 8 : 4);
	      sa << sep << "ts";
	      if (flag == 1)
		  sa << ".ip";
	      else if (flag == 3)
		  sa << ".preip";
	      else if (flag != 0)
		  sa << "." << flag;
	      sa << '{';
	      sep = "";
	      for (; o + size <= ol; o += 4) {
		  if (o == op) {
		      if (flag != 3)
			  break;
		      sep = "^";
		  }
		  if (flag != 0) {
		      sa << sep << (int)o[0] << '.' << (int)o[1] << '.' << (int)o[2] << '.' << (int)o[3] << '=';
		      o += 4;
		  } else
		      sa << sep;
		  if (flag == 3 && o > op)
		      sa.pop_back();
		  else {
		      uint32_t v = (o[0] << 24) | (o[1] << 16) | (o[2] << 8) | o[3];
		      if (v & 0x80000000U)
			  sa << '!';
		      sa << (v & 0x7FFFFFFFU);
		  }
		  sep = ",";
	      }
	      if (o == ol && o == op)
		  sa << '^';
	      sa << '}';
	      if (o + size <= ol && o == op)
		  sa << '+' << (ol - o) / size;
	      if (opt[3] & 0xF0)
		  sa << '+' << '+' << (opt[3] >> 4);
	      opt = ol;
	      sep = ";";
	      break;
	  }
	  default: {
	      if (opt + opt[1] > end_opt || opt[1] < 2)
		  goto bad_opt;
	      if (!(mask & DO_IPOPT_UNKNOWN))
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

void unparse_ip_opt(StringAccum& sa, const click_ip* iph, int mask)
{
    unparse_ip_opt(sa, reinterpret_cast<const uint8_t *>(iph + 1), (iph->ip_hl << 2) - sizeof(click_ip), mask);
}

void unparse_ip_opt_binary(StringAccum& sa, const uint8_t *opt, int opt_len, int mask)
{
    if (mask == (int)DO_IPOPT_ALL) {
	// store all options
	sa.append((char)opt_len);
	sa.append(opt, opt_len);
    }

    const uint8_t *end_opt = opt + opt_len;
    int initial_sa_len = sa.length();
    sa.append('\0');

    while (opt < end_opt) {
	// one-byte options
	if (*opt == IPOPT_EOL) {
	    if (mask & DO_IPOPT_PADDING)
		sa.append(opt, 1);
	    goto done;
	} else if (*opt == IPOPT_NOP) {
	    if (mask & DO_IPOPT_PADDING)
		sa.append(opt, 1);
	    opt++;
	    continue;
	}

	// quit copying options if you encounter something obviously invalid
	if (opt[1] < 2 || opt + opt[1] > end_opt)
	    break;

	int this_content = (*opt > IPOPT_RA ? (int)DO_IPOPT_UNKNOWN : ip_opt_mask_mapping[*opt]);
	if (mask & this_content)
	    sa.append(opt, opt[1]);
	opt += opt[1];
    }

  done:
    sa[initial_sa_len] = sa.length() - initial_sa_len - 1;
}

void unparse_ip_opt_binary(StringAccum& sa, const click_ip *iph, int mask)
{
    unparse_ip_opt_binary(sa, reinterpret_cast<const uint8_t *>(iph + 1), (iph->ip_hl << 2) - sizeof(click_ip), mask);
}

static void append_net_uint32_t(StringAccum &sa, uint32_t u)
{
    sa << (char)(u >> 24) << (char)(u >> 16) << (char)(u >> 8) << (char)u;
}

static bool ip_opt_ina(PacketOdesc &d, const String &str, const FieldReader *)
{
    if (!str || str.equals(".", 1))
	return true;
    else if (str.equals("-", 1))
	return false;
    const uint8_t *s = reinterpret_cast<const uint8_t *>(str.begin());
    const uint8_t *end = reinterpret_cast<const uint8_t *>(str.end());
    int contents = DO_IPOPT_ALL;
    d.sa.clear();

    while (1) {
	const unsigned char *t;
	uint32_t u1;

	if (s + 3 < end && memcmp(s, "rr{", 3) == 0
	    && (contents & DO_IPOPT_ROUTE)) {
	    // record route
	    d.sa << (char)IPOPT_RR;
	    s += 3;
	  parse_route:
	    int sa_pos = d.sa.length() - 1;
	    int pointer = -1;
	    d.sa << '\0' << '\0';
	    // loop over entries
	    while (1) {
		if (s < end && *s == '^' && pointer < 0)
		    pointer = d.sa.length() - sa_pos + 1, s++;
		if (s >= end || !isdigit(*s))
		    break;
		for (int i = 0; i < 4; i++) {
		    u1 = 256;
		    s = cp_integer(s, end, 10, &u1) + (i < 3);
		    if (u1 > 255 || (i < 3 && (s > end || s[-1] != '.')))
			goto bad_opt;
		    d.sa << (char)u1;
		}
		if (s < end && *s == ',')
		    s++;
	    }
	    if (s >= end || *s != '}') // must end with a brace
		goto bad_opt;
	    d.sa[sa_pos + 2] = (pointer >= 0 ? pointer : d.sa.length() - sa_pos + 1);
	    if (s + 2 < end && s[1] == '+' && isdigit(s[2])) {
		s = cp_integer(s + 2, end, 10, &u1);
		if (u1 < 64)
		    d.sa.append_fill('\0', u1 * 4);
	    } else
		s++;
	    if (d.sa.length() - sa_pos > 255)
		goto bad_opt;
	    d.sa[sa_pos + 1] = d.sa.length() - sa_pos;

	} else if (s + 5 < end && memcmp(s, "ssrr{", 5) == 0
		   && (contents & DO_IPOPT_ROUTE)) {
	    // strict source route option
	    d.sa << (char)IPOPT_SSRR;
	    s += 5;
	    goto parse_route;

	} else if (s + 5 < end && memcmp(s, "lsrr{", 5) == 0
		   && (contents & DO_IPOPT_ROUTE)) {
	    // loose source route option
	    d.sa << (char)IPOPT_LSRR;
	    s += 5;
	    goto parse_route;

	} else if (s + 3 < end
		   && (memcmp(s, "ts{", 3) == 0 || memcmp(s, "ts.", 3) == 0)
		   && (contents & DO_IPOPT_TS)) {
	    // timestamp option
	    int sa_pos = d.sa.length();
	    d.sa << (char)IPOPT_TS << (char)0 << (char)0 << (char)0;
	    uint32_t top_bit;
	    int flag = -1;
	    if (s[2] == '.') {
		if (s + 6 < end && memcmp(s + 3, "ip{", 3) == 0)
		    flag = 1, s += 6;
		else if (s + 9 < end && memcmp(s + 3, "preip{", 6) == 0)
		    flag = 3, s += 9;
		else if (isdigit(s[3])
			 && (t = cp_integer(s + 3, end, 0, (uint32_t *)&flag))
			 && flag <= 15 && t < end && *t == '{')
		    s = t + 1;
		else
		    goto bad_opt;
	    } else
		s += 3;
	    int pointer = -1;

	    // loop over timestamp entries
	    while (1) {
		if (s < end && *s == '^' && pointer < 0)
		    pointer = d.sa.length() - sa_pos + 1, s++;
		if (s >= end || (!isdigit(*s) && *s != '!'))
		    break;
		const unsigned char *entry = s;

	      retry_entry:
		if (flag == 1 || flag == 3 || flag == -2) {
		    // parse IP address
		    for (int i = 0; i < 4; i++) {
			u1 = 256;
			s = cp_integer(s, end, 10, &u1) + (i < 3);
			if (u1 > 255 || (i < 3 && (s > end || s[-1] != '.')))
			    goto bad_opt;
			d.sa << (char)u1;
		    }
		    // prespecified IPs if we get here
		    if (pointer >= 0 && flag == -2)
			flag = 3;
		    // check for valid value: either "=[DIGIT]", "=!", "=?"
		    // (for pointer >= 0)
		    if (s + 1 < end && *s == '=') {
			if (isdigit(s[1]) || s[1] == '!')
			    s++;
			else if (s[1] == '?' && pointer >= 0) {
			    d.sa << (char)0 << (char)0 << (char)0 << (char)0;
			    s += 2;
			    goto done_entry;
			} else
			    goto bad_opt;
		    } else if (pointer >= 0) {
			d.sa << (char)0 << (char)0 << (char)0 << (char)0;
			goto done_entry;
		    } else
			goto bad_opt;
		}

		// parse timestamp value
		assert(s < end);
		top_bit = 0;
		if (*s == '!')
		    top_bit = 0x80000000U, s++;
		if (s >= end || !isdigit(*s))
		    goto bad_opt;
		s = cp_integer(s, end, 0, &u1);
		if (s < end && *s == '.' && flag == -1) {
		    flag = -2;
		    s = entry;
		    goto retry_entry;
		} else if (flag == -1)
		    flag = 0;
		u1 |= top_bit;
		append_net_uint32_t(d.sa, u1);
	      done_entry:
		// check separator
		if (s < end && *s == ',')
		    s++;
	    }

	    // done with entries
	    if (s < end && *s++ != '}')
		goto bad_opt;
	    if (flag == -2)
		flag = 1;
	    d.sa[sa_pos + 2] = (pointer >= 0 ? pointer : d.sa.length() - sa_pos + 1);
	    if (s + 1 < end && *s == '+' && isdigit(s[1])
		&& (s = cp_integer(s + 1, end, 0, &u1))
		&& u1 < 64)
		d.sa.append_fill('\0', u1 * (flag == 1 || flag == 3 ? 8 : 4));
	    int overflow = 0;
	    if (s + 2 < end && *s == '+' && s[1] == '+' && isdigit(s[2])
		&& (s = cp_integer(s + 2, end, 0, &u1))
		&& u1 < 16)
		overflow = u1;
	    d.sa[sa_pos + 3] = (overflow << 4) | flag;
	    if (d.sa.length() - sa_pos > 255)
		goto bad_opt;
	    d.sa[sa_pos + 1] = d.sa.length() - sa_pos;

	} else if (s < end && isdigit(*s) && (contents & DO_IPOPT_UNKNOWN)) {
	    // unknown option
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
		   && (contents & DO_IPOPT_PADDING)) {
	    d.sa << (char)IPOPT_NOP;
	    s += 3;
	} else if (s + 3 <= end && memcmp(s, "eol", 3) == 0
		   && (contents & DO_IPOPT_PADDING)
		   && (s + 3 == end || s[3] != ',')) {
	    d.sa << (char)IPOPT_EOL;
	    s += 3;
	} else
	    goto bad_opt;

	if (s >= end) {
	    // check for improper padding
	    while (d.sa.length() > 40 && d.sa[0] == IPOPT_NOP) {
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

static const FieldWriter ip_writers[] = {
    { "ip_src", B_4NET, T_IP_SRC,
      ip_prepare, ip_extract, ip_outa, outb },
    { "ip_dst", B_4NET, T_IP_DST,
      ip_prepare, ip_extract, ip_outa, outb },
    { "ip_tos", B_1, T_IP_TOS,
      ip_prepare, ip_extract, num_outa, outb },
    { "ip_dscp", B_1, T_IP_DSCP,
      ip_prepare, ip_extract, num_outa, outb },
    { "ip_ecn", B_1, T_IP_ECN,
      ip_prepare, ip_extract, ip_outa, outb },
    { "ip_ttl", B_1, T_IP_TTL,
      ip_prepare, ip_extract, num_outa, outb },
    { "ip_frag", B_1, T_IP_FRAG,
      ip_prepare, ip_extract, ip_outa, outb },
    { "ip_fragoff", B_2, T_IP_FRAGOFF,
      ip_prepare, ip_extract, ip_outa, outb },
    { "ip_id", B_2, T_IP_ID,
      ip_prepare, ip_extract, num_outa, outb },
    { "ip_sum", B_2, T_IP_SUM,
      ip_prepare, ip_extract, num_outa, outb },
    { "ip_proto", B_1, T_IP_PROTO,
      ip_prepare, ip_extract, ip_outa, outb },
    { "ip_hl", B_1, T_IP_HL,
      ip_prepare, ip_extract, num_outa, outb },
    { "ip_len", B_4, T_IP_LEN,
      ip_prepare, ip_extract, num_outa, outb },
    { "ip_capture_len", B_4, T_IP_CAPTURE_LEN,
      ip_prepare, ip_extract, num_outa, outb },
    { "ip_opt", B_SPECIAL, T_IP_OPT,
      ip_prepare, ip_extract, ip_outa, ip_outb },
    { "sport", B_2, T_SPORT,
      ip_prepare, transport_extract, num_outa, outb },
    { "dport", B_2, T_DPORT,
      ip_prepare, transport_extract, num_outa, outb }
};

static const FieldReader ip_readers[] = {
    { "ip_src", B_4NET, T_IP_SRC, order_net,
      ip_ina, inb, ip_inject },
    { "ip_dst", B_4NET, T_IP_DST, order_net,
      ip_ina, inb, ip_inject },
    { "ip_tos", B_1, T_IP_TOS, order_net,
      num_ina, inb, ip_inject },
    { "ip_dscp", B_1, T_IP_DSCP, order_net,
      num_ina, inb, ip_inject },
    { "ip_ecn", B_1, T_IP_ECN, order_net,
      ip_ina, inb, ip_inject },
    { "ip_ttl", B_1, T_IP_TTL, order_net,
      num_ina, inb, ip_inject },
    { "ip_frag", B_1, T_IP_FRAG, order_net - 2,
      ip_ina, inb, ip_inject },
    { "ip_fragoff", B_2, T_IP_FRAGOFF, order_net - 1,
      ip_ina, inb, ip_inject },
    { "ip_id", B_2, T_IP_ID, order_net,
      num_ina, inb, ip_inject },
    { "ip_sum", B_2, T_IP_SUM, order_net + 2,
      num_ina, inb, ip_inject },
    { "ip_proto", B_1, T_IP_PROTO, order_net,
      ip_ina, inb, ip_inject },
    { "ip_hl", B_1, T_IP_HL, order_net - 1,
      num_ina, inb, ip_inject },
    { "ip_len", B_4, T_IP_LEN, order_net + 1,
      num_ina, inb, ip_inject },
    { "ip_capture_len", B_4, T_IP_CAPTURE_LEN, order_payload + 2,
      num_ina, inb, ip_inject },
    { "ip_opt", B_SPECIAL, T_IP_OPT, order_net,
      ip_opt_ina, ip_inb, ip_inject },
    { "sport", B_2, T_SPORT, order_transp,
      num_ina, inb, transport_inject },
    { "dport", B_2, T_DPORT, order_transp,
      num_ina, inb, transport_inject }
};

static const FieldSynonym ip_synonyms[] = {
    { "length", "ip_len" },
    { "ip_p", "ip_proto" }
};

}

void IPSummaryDump_IP::static_initialize()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(ip_writers) / sizeof(ip_writers[0]); ++i)
	FieldWriter::add(&ip_writers[i]);
    for (size_t i = 0; i < sizeof(ip_readers) / sizeof(ip_readers[0]); ++i)
	FieldReader::add(&ip_readers[i]);
    for (size_t i = 0; i < sizeof(ip_synonyms) / sizeof(ip_synonyms[0]); ++i)
	FieldSynonym::add(&ip_synonyms[i]);
}

void IPSummaryDump_IP::static_cleanup()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(ip_writers) / sizeof(ip_writers[0]); ++i)
	FieldWriter::remove(&ip_writers[i]);
    for (size_t i = 0; i < sizeof(ip_readers) / sizeof(ip_readers[0]); ++i)
	FieldReader::remove(&ip_readers[i]);
    for (size_t i = 0; i < sizeof(ip_synonyms) / sizeof(ip_synonyms[0]); ++i)
	FieldSynonym::remove(&ip_synonyms[i]);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_IP)
CLICK_ENDDECLS
