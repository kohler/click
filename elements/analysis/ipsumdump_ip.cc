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
#include <click/md5.h>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/confparse.hh>
CLICK_DECLS

enum { T_IP_SRC, T_IP_DST, T_IP_TOS, T_IP_TTL, T_IP_FRAG, T_IP_FRAGOFF,
       T_IP_ID, T_IP_SUM, T_IP_PROTO, T_IP_OPT, T_IP_LEN, T_IP_CAPTURE_LEN,
       T_SPORT, T_DPORT, T_PAYLOAD_LEN, T_PAYLOAD, T_PAYLOAD_MD5 };

namespace IPSummaryDump {

void ip_prepare(PacketDesc& d)
{
    Packet* p = d.p;
    d.iph = p->ip_header();
    d.tcph = p->tcp_header();
    d.udph = p->udp_header();
    
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
	    p->take(p->network_length() - ip_len);
	} else if (d.careful_trunc && p->network_length() + EXTRA_LENGTH_ANNO(p) < (uint32_t) ip_len) {
	    /* This doesn't actually kill the IP header. */ 
	    int scratch;
	    BAD2("truncated IP missing ", (ntohs(d.iph->ip_len) - p->network_length() - EXTRA_LENGTH_ANNO(p)), scratch);
	}
    }

    // check TCP header
    if (!d.iph || !d.tcph
	|| p->network_length() <= (int)(d.iph->ip_hl << 2)
	|| d.iph->ip_p != IP_PROTO_TCP
	|| !IP_FIRSTFRAG(d.iph))
	d.tcph = 0;
    else if (p->transport_length() > 12
	     && d.tcph->th_off < (sizeof(click_tcp) >> 2))
	BAD2("TCP header length ", d.tcph->th_off, d.tcph);

    // check UDP header
    if (!d.iph || !d.udph
	|| p->network_length() <= (int)(d.iph->ip_hl << 2)
	|| d.iph->ip_p != IP_PROTO_UDP
	|| !IP_FIRSTFRAG(d.iph))
	d.udph = 0;
#undef BAD
#undef BAD2

    // Adjust extra length, since we calculate lengths here based on ip_len.
    if (d.iph && EXTRA_LENGTH_ANNO(p) > 0) {
	int32_t full_len = p->length() + EXTRA_LENGTH_ANNO(p);
	if (ntohs(d.iph->ip_len) + 8 >= full_len - p->network_header_offset())
	    SET_EXTRA_LENGTH_ANNO(p, 0);
	else {
	    full_len = full_len - ntohs(d.iph->ip_len);
	    SET_EXTRA_LENGTH_ANNO(p, full_len);
	}
    }
}

static bool ip_extract(PacketDesc& d, int thunk)
{
    int network_length = d.p->network_length();
    switch (thunk & ~B_TYPEMASK) {

	// IP header properties
#define CHECK(l) do { if (!d.iph || network_length < (l)) return field_missing(d, MISSING_IP, "IP", (l)); } while (0)	
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
      case T_IP_TTL:
	CHECK(9);
	d.v = d.iph->ip_ttl;
	return true;
      case T_IP_FRAG:
	CHECK(8);
	d.v = (IP_ISFRAG(d.iph) ? (IP_FIRSTFRAG(d.iph) ? 'F' : 'f') : '.');
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
	if (!d.iph || (d.iph->ip_hl > 5 && network_length < (int)(d.iph->ip_hl << 2)))
	    return field_missing(d, MISSING_IP, "IP", (d.iph ? d.iph->ip_hl << 2 : 20));
	if (d.iph->ip_hl <= 5)
	    d.vptr = 0, d.v2 = 0;
	else {
	    d.vptr = (const uint8_t *) (d.iph + 1);
	    d.v2 = (d.iph->ip_hl << 2) - sizeof(click_ip);
	}
	return true;
      case T_IP_LEN:
	if (d.iph)
	    d.v = ntohs(d.iph->ip_len) + (d.force_extra_length ? EXTRA_LENGTH_ANNO(d.p) : 0);
	else
	    d.v = d.p->length() + EXTRA_LENGTH_ANNO(d.p);
	return true;
      case T_IP_CAPTURE_LEN: {
	  uint32_t allow_len = (d.iph ? network_length : d.p->length());
	  uint32_t len = (d.iph ? ntohs(d.iph->ip_len) : allow_len);
	  d.v = (len < allow_len ? len : allow_len);
	  return true;
      }
#undef CHECK

      default:
	return false;
    }
}

static void ip_outa(const PacketDesc& d, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
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
	break;
      case T_IP_PROTO:
	switch (d.v) {
	  case IP_PROTO_TCP:	*d.sa << 'T'; break;
	  case IP_PROTO_UDP:	*d.sa << 'U'; break;
	  case IP_PROTO_ICMP:	*d.sa << 'I'; break;
	  default:		*d.sa << d.v; break;
	}
	break;
      case T_IP_OPT:
	if (!d.vptr)
	    *d.sa << '.';
	else
	    unparse_ip_opt(*d.sa, d.vptr, d.v2, DO_IPOPT_ALL_NOPAD);
	break;
    }
}

static void ip_outb(const PacketDesc& d, bool ok, int thunk)
{
    if ((thunk & ~B_TYPEMASK) == T_IP_OPT) {
	if (!ok || !d.vptr)
	    *d.sa << '\0';
	else
	    unparse_ip_opt_binary(*d.sa, d.vptr, d.v2, DO_IPOPT_ALL);
    }
}

static const uint8_t* ip_inb(PacketDesc& d, const uint8_t *s, const uint8_t *ends, int thunk)
{
    if ((thunk & ~B_TYPEMASK) == T_IP_OPT && s + s[0] + 1 <= ends) {
	d.vptr = s + 1;
	d.v2 = s[0];
	return s + s[0] + 1;
    } else
	return ends;
}


static bool transport_extract(PacketDesc& d, int thunk)
{
    Packet* p = d.p;
    switch (thunk & ~B_TYPEMASK) {
	
	// TCP/UDP header properties
#define CHECK(l) do { if ((!d.tcph && !d.udph) || p->transport_length() < (l)) return field_missing(d, MISSING_IP_TRANSPORT, "transport", (l)); } while (0)
      case T_SPORT:
	CHECK(2);
	d.v = ntohs(p->udp_header()->uh_sport);
	return true;
      case T_DPORT:
	CHECK(4);
	d.v = ntohs(p->udp_header()->uh_dport);
	return true;
#undef CHECK

      case T_PAYLOAD_LEN:
	if (d.iph) {
	    d.v = ntohs(d.iph->ip_len);
	    int32_t off = p->network_header_length();
	    if (d.tcph && p->transport_length() >= 13
		&& off + (d.tcph->th_off << 2) <= d.v)
		off += (d.tcph->th_off << 2);
	    else if (d.udph)
		off += sizeof(click_udp);
	    else if (IP_FIRSTFRAG(d.iph) && (d.iph->ip_p == IP_PROTO_TCP || d.iph->ip_p == IP_PROTO_UDP))
		off = d.v;
	    d.v -= off;
	    d.v += (d.force_extra_length ? EXTRA_LENGTH_ANNO(p) : 0);
	} else
	    d.v = p->length() + EXTRA_LENGTH_ANNO(p);
	return true;
      case T_PAYLOAD:
      case T_PAYLOAD_MD5:
	return true;

      default:
	return false;
    }
}

static void payload_info(const PacketDesc &d, int32_t &off, uint32_t &len)
{
    if (d.iph) {
	len = ntohs(d.iph->ip_len);
	off = d.p->transport_header_offset();
	if (d.tcph && d.p->transport_length() >= 13
	    && off + (d.tcph->th_off << 2) <= len)
	    off += (d.tcph->th_off << 2);
	else if (d.udph)
	    off += sizeof(click_udp);
	len = len - off + d.p->network_header_offset();
	if (len + off > d.p->length()) // EXTRA_LENGTH?
	    len = d.p->length() - off;
    } else {
	off = 0;
	len = d.p->length();
    }
}

static void transport_outa(const PacketDesc& d, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
      case T_PAYLOAD:
      case T_PAYLOAD_MD5: {
	  int32_t off;
	  uint32_t len;
	  payload_info(d, off, len);
	  if ((thunk & ~B_TYPEMASK) == T_PAYLOAD) {
	      String s = String::stable_string((const char *)(d.p->data() + off), len);
	      *d.sa << cp_quote(s);
	  } else {
	      md5_state_t pms;
	      md5_init(&pms);
	      md5_append(&pms, (const md5_byte_t *) (d.p->data() + off), len);
	      if (char *buf = d.sa->extend(MD5_TEXT_DIGEST_SIZE))
		  md5_finish_text(&pms, buf, 1);
	      md5_free(&pms);
	  }
	  break;
      }
    }
} 

static void transport_outb(const PacketDesc& d, bool, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
      case T_PAYLOAD_MD5: {
	  int32_t off;
	  uint32_t len;
	  payload_info(d, off, len);
	  md5_state_t pms;
	  md5_init(&pms);
	  md5_append(&pms, (const md5_byte_t *) (d.p->data() + off), len);
	  if (char *buf = d.sa->extend(MD5_DIGEST_SIZE))
	      md5_finish(&pms, (md5_byte_t *) buf);
	  md5_free(&pms);
	  break;
      }
    }
} 



#define U DO_IPOPT_UNKNOWN
static int ip_opt_mask_mapping[] = {
    DO_IPOPT_PADDING, DO_IPOPT_PADDING,		// EOL, NOP
    U, U, U, U,	U,				// 2, 3, 4, 5, 6
    DO_IPOPT_ROUTE,				// RR
    U, U, U, U, U, U, U, U, U, U, U, U, U, 	// 8-20
    U, U, U, U, U, U, U, U, U, U, 		// 21-30
    U, U, U, U, U, U, U, U, U, U, 		// 31-40
    U, U, U, U, U, U, U, U, U, U, 		// 41-50
    U, U, U, U, U, U, U, U, U, U, 		// 51-60
    U, U, U, U, U, U, U,	 		// 61-67
    DO_IPOPT_TS, U, U,				// TS, 69-70
    U, U, U, U, U, U, U, U, U, U, 		// 71-80
    U, U, U, U, U, U, U, U, U, U, 		// 81-90
    U, U, U, U, U, U, U, U, U, U, 		// 91-100
    U, U, U, U, U, U, U, U, U, U, 		// 101-110
    U, U, U, U, U, U, U, U, U, U, 		// 111-120
    U, U, U, U, U, U, U, U, U,			// 121-129
    DO_IPOPT_UNKNOWN, DO_IPOPT_ROUTE,		// SECURITY, LSRR
    U, U, U, U,					// 132-135
    DO_IPOPT_UNKNOWN, DO_IPOPT_ROUTE, 		// SATID, SSRR
    U, U, U,					// 138-140
    U, U, U, U, U, U, U,	 		// 141-147
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


void ip_register_unparsers()
{
    register_unparser("ip_src", T_IP_SRC | B_4NET, ip_prepare, ip_extract, ip_outa, outb, inb);
    register_unparser("ip_dst", T_IP_DST | B_4NET, ip_prepare, ip_extract, ip_outa, outb, inb);
    register_unparser("ip_tos", T_IP_TOS | B_1, ip_prepare, ip_extract, num_outa, outb, inb);
    register_unparser("ip_ttl", T_IP_TTL | B_1, ip_prepare, ip_extract, num_outa, outb, inb);
    register_unparser("ip_frag", T_IP_FRAG | B_1, ip_prepare, ip_extract, ip_outa, outb, inb);
    register_unparser("ip_fragoff", T_IP_FRAGOFF | B_2, ip_prepare, ip_extract, ip_outa, outb, inb);
    register_unparser("ip_id", T_IP_ID | B_2, ip_prepare, ip_extract, num_outa, outb, inb);
    register_unparser("ip_sum", T_IP_SUM | B_2, ip_prepare, ip_extract, num_outa, outb, inb);
    register_unparser("ip_proto", T_IP_PROTO | B_1, ip_prepare, ip_extract, ip_outa, outb, inb);
    register_unparser("ip_len", T_IP_LEN | B_4, ip_prepare, ip_extract, num_outa, outb, inb);
    register_unparser("ip_capture_len", T_IP_CAPTURE_LEN | B_4, ip_prepare, ip_extract, num_outa, outb, inb);
    register_unparser("ip_opt", T_IP_OPT | B_SPECIAL, ip_prepare, ip_extract, ip_outa, ip_outb, ip_inb);

    register_unparser("sport", T_SPORT | B_2, ip_prepare, transport_extract, num_outa, outb, inb);
    register_unparser("dport", T_DPORT | B_2, ip_prepare, transport_extract, num_outa, outb, inb);
    register_unparser("payload_len", T_PAYLOAD_LEN | B_4, ip_prepare, transport_extract, num_outa, outb, inb);
    register_unparser("payload", T_PAYLOAD | B_NOTALLOWED, ip_prepare, transport_extract, transport_outa, 0, 0);
    register_unparser("payload_md5", T_PAYLOAD_MD5 | B_16, ip_prepare, transport_extract, transport_outa, transport_outb, 0);

    register_synonym("length", "ip_len");
    register_synonym("ip_p", "ip_proto");
    register_synonym("payload_length", "payload_len");
}

}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_IP)
CLICK_ENDDECLS
