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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
CLICK_DECLS

enum { T_TCP_SEQ, T_TCP_ACK, T_TCP_FLAGS, T_TCP_WINDOW, T_TCP_URP, T_TCP_OPT,
       T_TCP_NTOPT, T_TCP_SACK };

namespace IPSummaryDump {

static bool tcp_extract(PacketDesc& d, int thunk)
{
    int transport_length = d.p->transport_length();
    switch (thunk & ~B_TYPEMASK) {
	
#define CHECK(l) do { if (!d.tcph || transport_length < (l)) return field_missing(d, MISSING_IP_TRANSPORT, "TCP", (l)); } while (0)
	
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
	if (d.tcph && transport_length >= 13
	    && (d.tcph->th_off <= 5 || transport_length >= (int)(d.tcph->th_off << 2)))
	    return true;
	else
	    return field_missing(d, MISSING_IP_TRANSPORT, "TCP", transport_length + 1);
      case T_TCP_NTOPT:
      case T_TCP_SACK:
	// need to check that d.tcph->th_off exists
	if (d.tcph && transport_length >= 13
	    && (d.tcph->th_off <= 5
		|| transport_length >= (int)(d.tcph->th_off << 2)
		|| (d.tcph->th_off == 8 && transport_length >= 24 && *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A))))
	    return true;
	else
	    return field_missing(d, MISSING_IP_TRANSPORT, "TCP", transport_length + 1);
	
#undef CHECK

      default:
	return false;
    }
}

static void tcp_outa(const PacketDesc& d, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
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
	if (d.tcph->th_off <= 5)
	    *d.sa << '.';
	else
	    unparse_tcp_opt(*d.sa, d.tcph, DO_TCPOPT_ALL_NOPAD);
	break;
      case T_TCP_NTOPT:
	if (d.tcph->th_off <= 5
	    || (d.tcph->th_off == 8
		&& *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
	    *d.sa << '.';
	else
	    unparse_tcp_opt(*d.sa, d.tcph, DO_TCPOPT_NTALL);
	break;
      case T_TCP_SACK:
	if (d.tcph->th_off <= 5
	    || (d.tcph->th_off == 8
		&& *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
	    *d.sa << '.';
	else
	    unparse_tcp_opt(*d.sa, d.tcph, DO_TCPOPT_SACK);
	break;
    }
}

static void tcp_outb(const PacketDesc& d, bool ok, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
      case T_TCP_OPT:
	if (!ok || d.tcph->th_off <= (sizeof(click_tcp) >> 2))
	    *d.sa << '\0';
	else
	    unparse_tcp_opt_binary(*d.sa, d.tcph, DO_TCPOPT_ALL);
	break;
      case T_TCP_NTOPT:
	if (!ok || d.tcph->th_off <= (sizeof(click_tcp) >> 2)
	    || (d.tcph->th_off == 8
		&& *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
	    *d.sa << '\0';
	else
	    unparse_tcp_opt_binary(*d.sa, d.tcph, DO_TCPOPT_NTALL);
	break;
      case T_TCP_SACK:
	if (!ok || d.tcph->th_off <= (sizeof(click_tcp) >> 2)
	    || (d.tcph->th_off == 8
		&& *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
	    *d.sa << '\0';
	else
	    unparse_tcp_opt_binary(*d.sa, d.tcph, DO_TCPOPT_SACK);
	break;
    }
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


void tcp_register_unparsers()
{
    register_unparser("tcp_seq", T_TCP_SEQ | B_4, ip_prepare, tcp_extract, num_outa, outb);
    register_unparser("tcp_ack", T_TCP_ACK | B_4, ip_prepare, tcp_extract, num_outa, outb);
    register_unparser("tcp_flags", T_TCP_FLAGS | B_1, ip_prepare, tcp_extract, tcp_outa, outb);
    register_unparser("tcp_window", T_TCP_WINDOW | B_2, ip_prepare, tcp_extract, num_outa, outb);
    register_unparser("tcp_urp", T_TCP_URP | B_2, ip_prepare, tcp_extract, num_outa, outb);
    register_unparser("tcp_opt", T_TCP_OPT | B_SPECIAL, ip_prepare, tcp_extract, tcp_outa, tcp_outb);
    register_unparser("tcp_ntopt", T_TCP_NTOPT | B_SPECIAL, ip_prepare, tcp_extract, tcp_outa, tcp_outb);
    register_unparser("tcp_sack", T_TCP_SACK | B_SPECIAL, ip_prepare, tcp_extract, tcp_outa, tcp_outb);

    register_synonym("tcp_seqno", "tcp_seq");
    register_synonym("tcp_ackno", "tcp_ack");
    register_synonym("tcp_win", "tcp_window");
}

}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_TCP)
CLICK_ENDDECLS
