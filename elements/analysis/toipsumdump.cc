// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * toipsummarydump.{cc,hh} -- element writes packet summary in ASCII
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include "toipsumdump.hh"
#include <click/standard/scheduleinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <unistd.h>
#include <time.h>
CLICK_DECLS

#ifdef i386
# define PUT4NET(p, d)	*reinterpret_cast<uint32_t *>((p)) = (d)
# define PUT4(p, d)	*reinterpret_cast<uint32_t *>((p)) = htonl((d))
# define PUT2(p, d)	*reinterpret_cast<uint16_t *>((p)) = htons((d))
#else
# define PUT4NET(p, d)	do { uint32_t d__ = ntohl((d)); (p)[0] = d__>>24; (p)[1] = d__>>16; (p)[2] = d__>>8; (p)[3] = d__; } while (0)
# define PUT4(p, d)	do { (p)[0] = (d)>>24; (p)[1] = (d)>>16; (p)[2] = (d)>>8; (p)[3] = (d); } while (0)
# define PUT2(p, d)	do { (p)[0] = (d)>>8; (p)[1] = (d); } while (0)
#endif
#define PUT1(p, d)	((p)[0] = (d))

ToIPSummaryDump::ToIPSummaryDump()
    : Element(1, 0), _f(0), _task(this)
{
    MOD_INC_USE_COUNT;
}

ToIPSummaryDump::~ToIPSummaryDump()
{
    MOD_DEC_USE_COUNT;
}

int
ToIPSummaryDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int before = errh->nerrors();
    String save = "timestamp ip_src";
    bool verbose = false;
    bool bad_packets = false;
    bool careful_trunc = true;
    bool multipacket = false;
    bool binary = false;

    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump filename", &_filename,
		    cpKeywords,
		    "CONTENTS", cpArgument, "log contents", &save,
		    "DATA", cpArgument, "log contents", &save,
		    "VERBOSE", cpBool, "be verbose?", &verbose,
		    "BANNER", cpString, "banner", &_banner,
		    "MULTIPACKET", cpBool, "output multiple packets based on packet count anno?", &multipacket,
		    "BAD_PACKETS", cpBool, "output '!bad' messages for non-IP or bad IP packets?", &bad_packets,
		    "CAREFUL_TRUNC", cpBool, "output '!bad' messages for truncated IP packets?", &careful_trunc,
		    "BINARY", cpBool, "output binary data?", &binary,
		    cpEnd) < 0)
	return -1;

    Vector<String> v;
    cp_spacevec(save, v);
    _binary_size = 4;
    for (int i = 0; i < v.size(); i++) {
	String word = cp_unquote(v[i]);
	int what = parse_content(word);
	if (what >= W_NONE && what < W_LAST) {
	    _contents.push_back(what);
	    int s = content_binary_size(what);
	    if (s == 4 || s == 8)
		_binary_size = (_binary_size + 3) & ~3;
	    else if (s == 2)
		_binary_size = (_binary_size + 1) & ~1;
	    else if (s < 0 && binary)
		errh->error("cannot use CONTENTS %s with BINARY", word.cc());
	    _binary_size += s;
	} else
	    errh->error("unknown content type '%s'", word.cc());
    }
    if (_contents.size() == 0)
	errh->error("no contents specified");

    // remove _multipacket if packet count specified
    for (int i = 0; i < _contents.size(); i++)
	if (_contents[i] == W_COUNT)
	    _multipacket = false;
    
    _verbose = verbose;
    _bad_packets = bad_packets;
    _careful_trunc = careful_trunc;
    _multipacket = multipacket;
    _binary = binary;

    return (before == errh->nerrors() ? 0 : -1);
}

int
ToIPSummaryDump::initialize(ErrorHandler *errh)
{
    assert(!_f);
    if (_filename != "-") {
	_f = fopen(_filename.c_str(), "wb");
	if (!_f)
	    return errh->error("%s: %s", _filename.cc(), strerror(errno));
    } else {
	_f = stdout;
	_filename = "<stdout>";
    }

    if (input_is_pull(0)) {
	ScheduleInfo::join_scheduler(this, &_task, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    _active = true;
    _output_count = 0;

    // magic number
    StringAccum sa;
    sa << "!IPSummaryDump " << MAJOR_VERSION << '.' << MINOR_VERSION << '\n';

    if (_banner)
	sa << "!creator " << cp_quote(_banner) << '\n';
    
    // host and start time
    if (_verbose) {
	char buf[BUFSIZ];
	buf[BUFSIZ - 1] = '\0';	// ensure NUL-termination
	if (gethostname(buf, BUFSIZ - 1) >= 0)
	    sa << "!host " << buf << '\n';

	time_t when = time(0);
	const char *cwhen = ctime(&when);
	struct timeval tv;
	if (gettimeofday(&tv, 0) >= 0)
	    sa << "!runtime " << tv << " (" << String(cwhen, strlen(cwhen) - 1) << ")\n";
    }

    // data description
    sa << "!data ";
    for (int i = 0; i < _contents.size(); i++)
	sa << (i ? " " : "") << unparse_content(_contents[i]);
    sa << '\n';

    // binary marker
    if (_binary)
	sa << "!binary\n";

    // print output
    fwrite(sa.data(), 1, sa.length(), _f);

    return 0;
}

void
ToIPSummaryDump::cleanup(CleanupStage)
{
    if (_f && _f != stdout)
	fclose(_f);
    _f = 0;
}

bool
ToIPSummaryDump::bad_packet(StringAccum &sa, const String &s, int what) const
{
    assert(_bad_packets);
    int pos = s.find_left("%d");
    if (pos >= 0)
	sa << "!bad " << s.substring(0, pos) << what
	   << s.substring(pos + 2) << '\n';
    else
	sa << "!bad " << s << '\n';
    return false;
}

void
ToIPSummaryDump::store_ip_opt_ascii(const uint8_t *opt, int opt_len, int contents, StringAccum &sa)
{
    int initial_sa_len = sa.length();
    const uint8_t *end_opt = opt + opt_len;
    const char *sep = "";
    
    while (opt < end_opt)
	switch (*opt) {
	  case IPOPT_EOL:
	    if (contents & DO_IPOPT_PADDING)
		sa << sep << "eol";
	    goto done;
	  case IPOPT_NOP:
	    if (contents & DO_IPOPT_PADDING) {
		sa << sep << "nop";
		sep = ";";
	    }
	    opt++;
	    break;
	  case IPOPT_RR:
	    if (opt + opt[1] > end_opt || opt[1] < 3 || opt[2] < 4)
		goto bad_opt;
	    if (!(contents & DO_IPOPT_ROUTE))
		goto unknown;
	    sa << sep << "rr";
	    goto print_route;
	  case IPOPT_LSRR:
	    if (opt + opt[1] > end_opt || opt[1] < 3 || opt[2] < 4)
		goto bad_opt;
	    if (!(contents & DO_IPOPT_ROUTE))
		goto unknown;
	    sa << sep << "lsrr";
	    goto print_route;
	  case IPOPT_SSRR:
	    if (opt + opt[1] > end_opt || opt[1] < 3 || opt[2] < 4)
		goto bad_opt;
	    if (!(contents & DO_IPOPT_ROUTE))
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
	      if (!(contents & DO_IPOPT_UNKNOWN))
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

inline void
ToIPSummaryDump::store_ip_opt_ascii(const click_ip *iph, int contents, StringAccum &sa)
{
    store_ip_opt_ascii(reinterpret_cast<const uint8_t *>(iph + 1), (iph->ip_hl << 2) - sizeof(click_ip), contents, sa);
}

void
ToIPSummaryDump::store_tcp_opt_ascii(const uint8_t *opt, int opt_len, int contents, StringAccum &sa)
{
    int initial_sa_len = sa.length();
    const uint8_t *end_opt = opt + opt_len;
    const char *sep = "";
    
    while (opt < end_opt)
	switch (*opt) {
	  case TCPOPT_EOL:
	    if (contents & DO_TCPOPT_PADDING)
		sa << sep << "eol";
	    goto done;
	  case TCPOPT_NOP:
	    if (contents & DO_TCPOPT_PADDING) {
		sa << sep << "nop";
		sep = ";";
	    }
	    opt++;
	    break;
	  case TCPOPT_MAXSEG:
	    if (opt + opt[1] > end_opt || opt[1] != TCPOLEN_MAXSEG)
		goto bad_opt;
	    if (!(contents & DO_TCPOPT_MSS))
		goto unknown;
	    sa << sep << "mss" << ((opt[2] << 8) | opt[3]);
	    opt += TCPOLEN_MAXSEG;
	    sep = ";";
	    break;
	  case TCPOPT_WSCALE:
	    if (opt + opt[1] > end_opt || opt[1] != TCPOLEN_WSCALE)
		goto bad_opt;
	    if (!(contents & DO_TCPOPT_WSCALE))
		goto unknown;
	    sa << sep << "wscale" << (int)(opt[2]);
	    opt += TCPOLEN_WSCALE;
	    sep = ";";
	    break;
	  case TCPOPT_SACK_PERMITTED:
	    if (opt + opt[1] > end_opt || opt[1] != TCPOLEN_SACK_PERMITTED)
		goto bad_opt;
	    if (!(contents & DO_TCPOPT_SACK))
		goto unknown;
	    sa << sep << "sackok";
	    opt += TCPOLEN_SACK_PERMITTED;
	    sep = ";";
	    break;
	  case TCPOPT_SACK: {
	      if (opt + opt[1] > end_opt || (opt[1] % 8 != 2))
		  goto bad_opt;
	      if (!(contents & DO_TCPOPT_SACK))
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
	      if (!(contents & DO_TCPOPT_TIMESTAMP))
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
	      if (!(contents & DO_TCPOPT_UNKNOWN))
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

inline void
ToIPSummaryDump::store_tcp_opt_ascii(const click_tcp *tcph, int contents, StringAccum &sa)
{
    store_tcp_opt_ascii(reinterpret_cast<const uint8_t *>(tcph + 1), (tcph->th_off << 2) - sizeof(click_tcp), contents, sa);
}

struct ToIPSummaryDump::PacketDesc {
    Packet *p;
    const click_ip *iph;
    const click_tcp *tcph;
    const click_udp *udph;
};

bool
ToIPSummaryDump::missing_field_message(const PacketDesc& d, const char* header_name, int l, StringAccum* bad_sa) const
{
    if (bad_sa && !*bad_sa) {
	*bad_sa << "!bad ";
	if (!d.iph)
	    *bad_sa << "no IP header";
	else if (!header_name)
	    *bad_sa << "truncated IP header capture";
	else if ((int) (d.p->transport_length() + EXTRA_LENGTH_ANNO(d.p)) >= l)
	    *bad_sa << "truncated " << header_name << " header capture";
	else if (IP_ISFRAG(d.iph))
	    *bad_sa << "fragmented " << header_name << " header";
	else
	    *bad_sa << "truncated " << header_name << " header";
	*bad_sa << '\n';
    }
    return false;
}

bool
ToIPSummaryDump::extract(PacketDesc& d, unsigned content, uint32_t& v, uint32_t& v2, StringAccum* bad_sa) const
{
    Packet *p = d.p;
    switch (content) {

	// general properties of packets
      case W_TIMESTAMP:
	v = p->timestamp_anno().tv_sec;
	v2 = p->timestamp_anno().tv_usec;
	return true;
      case W_TIMESTAMP_SEC:
	v = p->timestamp_anno().tv_sec;
	return true;
      case W_TIMESTAMP_USEC:
	v = p->timestamp_anno().tv_usec;
	return true;
      case W_TIMESTAMP_USEC1: {
#if HAVE_INT64_TYPES
	  uint64_t v3 = ((uint64_t)p->timestamp_anno().tv_sec * 1000000) + p->timestamp_anno().tv_usec;
	  v = v3 >> 32;
	  v2 = v3;
	  return true;
#else
	  // XXX silently output garbage if 64-bit ints not supported
	  v = 0;
	  v2 = (p->timestamp_anno().tv_sec * 1000000) + p->timestamp_anno().tv_usec;
	  return true;
#endif
      }
      case W_FIRST_TIMESTAMP:
	v = FIRST_TIMESTAMP_ANNO(p).tv_sec;
	v2 = FIRST_TIMESTAMP_ANNO(p).tv_usec;
	return true;
      case W_COUNT:
	v = 1 + EXTRA_PACKETS_ANNO(p);
	return true;
      case W_LINK:
	v = PAINT_ANNO(p);
	return true;
      case W_AGGREGATE:
	v = AGGREGATE_ANNO(p);
	return true;

	// IP header properties
#define CHECK(l) do { if (!d.iph || p->network_length() < (l)) return missing_field_message(d, 0, (l), bad_sa); } while (0)
	
      case W_IP_SRC:
	CHECK(16);
	v = d.iph->ip_src.s_addr;
	return true;
      case W_IP_DST:
	CHECK(20);
	v = d.iph->ip_dst.s_addr;
	return true;
      case W_IP_TOS:
	CHECK(2);
	v = d.iph->ip_tos;
	return true;
      case W_IP_TTL:
	CHECK(9);
	v = d.iph->ip_ttl;
	return true;
      case W_IP_FRAG:
	CHECK(8);
	v = (IP_ISFRAG(d.iph) ? (IP_FIRSTFRAG(d.iph) ? 'F' : 'f') : '.');
	return true;
      case W_IP_FRAGOFF:
	CHECK(8);
	v = ntohs(d.iph->ip_off);
	return true;
      case W_IP_ID:
	CHECK(6);
	v = ntohs(d.iph->ip_id);
	return true;
      case W_IP_PROTO:
	CHECK(10);
	v = d.iph->ip_p;
	return true;
      case W_IP_OPT:
	if (d.iph && (d.iph->ip_hl <= 5 || p->network_length() >= (int)(d.iph->ip_hl << 2)))
	    return true;
	else
	    return missing_field_message(d, 0, d.iph->ip_hl << 2, bad_sa);

	// TCP/UDP header properties
#undef CHECK
#define CHECK(l) do { if ((!d.tcph && !d.udph) || p->transport_length() < (l)) return missing_field_message(d, "transport", (l), bad_sa); } while (0)
	
      case W_SPORT:
	CHECK(2);
	v = ntohs(p->udp_header()->uh_sport);
	return true;
      case W_DPORT:
	CHECK(4);
	v = ntohs(p->udp_header()->uh_dport);
	return true;

	// TCP header properties
#undef CHECK
#define CHECK(l) do { if (!d.tcph || p->transport_length() < (l)) return missing_field_message(d, "TCP", (l), bad_sa); } while (0)
	
      case W_TCP_SEQ:
	CHECK(8);
	v = ntohl(d.tcph->th_seq);
	return true;
      case W_TCP_ACK:
	CHECK(12);
	v = ntohl(d.tcph->th_ack);
	return true;
      case W_TCP_FLAGS:
	CHECK(14);
	v = d.tcph->th_flags | (d.tcph->th_flags2 << 8);
	return true;
      case W_TCP_WINDOW:
	CHECK(16);
	v = ntohs(d.tcph->th_win);
	return true;
      case W_TCP_URP:
	CHECK(20);
	v = ntohs(d.tcph->th_urp);
	return true;
      case W_TCP_OPT:
	// need to check that d.tcph->th_off exists
	if (d.tcph && p->transport_length() >= 13
	    && (d.tcph->th_off <= 5 || p->transport_length() >= (int)(d.tcph->th_off << 2)))
	    return true;
	else
	    return missing_field_message(d, "TCP", p->transport_length() + 1, bad_sa);
      case W_TCP_NTOPT:
      case W_TCP_SACK:
	// need to check that d.tcph->th_off exists
	if (d.tcph && p->transport_length() >= 13
	    && (d.tcph->th_off <= 5
		|| p->transport_length() >= (int)(d.tcph->th_off << 2)
		|| (d.tcph->th_off == 8 && p->transport_length() >= 24 && *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A))))
	    return true;
	else
	    return missing_field_message(d, "TCP", p->transport_length() + 1, bad_sa);
	
#undef CHECK_ERROR
#undef CHECK

	// lengths
      case W_IP_LEN:
	v = (d.iph ? ntohs(d.iph->ip_len) : p->length())
	    + EXTRA_LENGTH_ANNO(p);
	return true;
      case W_PAYLOAD_LEN:
	if (d.iph) {
	    int32_t off = p->transport_header_offset();
	    if (d.tcph && p->transport_length() >= 13)
		off += (d.tcph->th_off << 2);
	    else if (d.udph)
		off += sizeof(click_udp);
	    else if (IP_FIRSTFRAG(d.iph) && (d.iph->ip_p == IP_PROTO_TCP || d.iph->ip_p == IP_PROTO_UDP))
		off = ntohs(d.iph->ip_len) + p->network_header_offset();
	    v = ntohs(d.iph->ip_len) + p->network_header_offset() - off;
	} else
	    v = p->length();
	v += EXTRA_LENGTH_ANNO(p);
	return true;
      case W_IP_CAPTURE_LEN: {
	  uint32_t allow_len = (d.iph ? p->network_length() : p->length());
	  uint32_t len = (d.iph ? ntohs(d.iph->ip_len) : allow_len);
	  v = (len < allow_len ? len : allow_len);
	  return true;
      }
      case W_PAYLOAD:
	return true;
	
      default:
	return false;
    }
}

bool
ToIPSummaryDump::summary(Packet* p, StringAccum& sa, StringAccum* bad_sa) const
{
    // Not all of these will be valid, but we calculate them just once.
    PacketDesc d;
    d.p = p;
    d.iph = p->ip_header();
    d.tcph = p->tcp_header();
    d.udph = p->udp_header();

    uint32_t v, v2;
    
#define BAD(msg, hdr) do { if (bad_sa && !*bad_sa) *bad_sa << "!bad " << msg << '\n'; hdr = 0; } while (0)
#define BAD2(msg, val, hdr) do { if (bad_sa && !*bad_sa) *bad_sa << "!bad " << msg << val << '\n'; hdr = 0; } while (0)
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
	    // XXX should we adjust EXTRA_LENGTH here?  SET_EXTRA_LENGTH_ANNO(p, EXTRA_LENGTH_ANNO(p) + p->network_length() - ip_len);
	    p->take(p->network_length() - ip_len);
	}
	if (_careful_trunc && p->network_length() + EXTRA_LENGTH_ANNO(p) < (uint32_t)(ntohs(d.iph->ip_len)))
	    /* This doesn't actually kill the IP header. */ 
	    BAD2("truncated IP missing ", (ntohs(d.iph->ip_len) - p->network_length() - EXTRA_LENGTH_ANNO(p)), v);
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

    // Binary output if you don't like.
    if (_binary)
	return binary_summary(d, sa, bad_sa);

    // Print actual contents.
    for (int i = 0; i < _contents.size(); i++) {
	if (i)
	    sa << ' ';

	if (!extract(d, _contents[i], v, v2, bad_sa)) {
	    sa << '-';
	    continue;
	}
	
	switch (_contents[i]) {
	  case W_TIMESTAMP:
	  case W_FIRST_TIMESTAMP: {
	      timeval tv = make_timeval(v, v2);
	      sa << tv;
	      break;
	  }
	  case W_TIMESTAMP_SEC:
	  case W_TIMESTAMP_USEC:
	  case W_IP_TOS:
	  case W_IP_TTL:
	  case W_SPORT:
	  case W_DPORT:
	  case W_IP_ID:
	  case W_TCP_SEQ:
	  case W_TCP_ACK:
	  case W_TCP_WINDOW:
	  case W_TCP_URP:
	  case W_IP_LEN:
	  case W_PAYLOAD_LEN:
	  case W_IP_CAPTURE_LEN:
	  case W_COUNT:
	  case W_AGGREGATE:
	    sa << v;
	    break;
	  case W_IP_SRC:
	  case W_IP_DST:
	    sa << IPAddress(v);
	    break;
	  case W_TIMESTAMP_USEC1:
#if HAVE_INT64_TYPES
	    sa << (((uint64_t)v) << 32) | v2;
#else
	    sa << v2;
#endif
	    break;
	  case W_IP_FRAG:
	    sa << (char) v;
	    break;
	  case W_IP_FRAGOFF:
	    sa << ((v & IP_OFFMASK) << 3);
	    if (v & IP_MF)
		sa << '+';
	    break;
	  case W_IP_PROTO:
	    switch (v) {
	      case IP_PROTO_TCP:	sa << 'T'; break;
	      case IP_PROTO_UDP:	sa << 'U'; break;
	      case IP_PROTO_ICMP:	sa << 'I'; break;
	      default:			sa << v; break;
	    }
	    break;
	  case W_IP_OPT:
	    if (d.iph->ip_hl <= 5)
		sa << '.';
	    else
		store_ip_opt_ascii(d.iph, DO_IPOPT_ALL_NOPAD, sa);
	    break;
	  case W_TCP_FLAGS:
	    if (v == (TH_ACK | TH_PUSH))
		sa << 'P' << 'A';
	    else if (v == TH_ACK)
		sa << 'A';
	    else if (v == 0)
		sa << '.';
	    else
		for (int flag = 0; flag < 9; flag++)
		    if (v & (1 << flag))
			sa << tcp_flags_word[flag];
	    break;
	  case W_TCP_OPT:
	    if (d.tcph->th_off <= 5)
		sa << '.';
	    else
		store_tcp_opt_ascii(d.tcph, DO_TCPOPT_ALL_NOPAD, sa);
	    break;
	  case W_TCP_NTOPT:
	    if (d.tcph->th_off <= 5
		|| (d.tcph->th_off == 8
		    && *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
		sa << '.';
	    else
		store_tcp_opt_ascii(d.tcph, DO_TCPOPT_NTALL, sa);
	    break;
	  case W_TCP_SACK:
	    if (d.tcph->th_off <= 5
		|| (d.tcph->th_off == 8
		    && *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
		sa << '.';
	    else
		store_tcp_opt_ascii(d.tcph, DO_TCPOPT_SACK, sa);
	    break;
	  case W_PAYLOAD: {
	      int32_t off;
	      uint32_t len;
	      if (d.iph) {
		  off = p->transport_header_offset();
		  if (d.tcph && p->transport_length() >= 13)
		      off += (d.tcph->th_off << 2);
		  else if (d.udph)
		      off += sizeof(click_udp);
		  len = ntohs(d.iph->ip_len) - off + p->network_header_offset();
		  if (len + off > p->length()) // EXTRA_LENGTH?
		      len = p->length() - off;
	      } else {
		  off = 0;
		  len = p->length();
	      }
	      String s = String::stable_string((const char *)(p->data() + off), len);
	      sa << cp_quote(s);
	      break;
	  }
	  case W_LINK:
	    if (v == 0)
		sa << '>';
	    else if (v == 1)
		sa << '<';
	    else
		sa << v;
	    break;
	  default:
	    sa << '-';
	    break;
	}
    }
    
    sa << '\n';
    return true;
}

#undef BAD_IP
#undef BAD_TCP
#undef BAD_UDP

#define T ToIPSummaryDump
#define U ToIPSummaryDump::DO_IPOPT_UNKNOWN
static int ip_opt_contents_mapping[] = {
    T::DO_IPOPT_PADDING, T::DO_IPOPT_PADDING,	// EOL, NOP
    U, U, U, U,	U,				// 2, 3, 4, 5, 6
    T::DO_IPOPT_ROUTE,				// RR
    U, U, U, U, U, U, U, U, U, U, U, U, U, 	// 8-20
    U, U, U, U, U, U, U, U, U, U, 		// 21-30
    U, U, U, U, U, U, U, U, U, U, 		// 31-40
    U, U, U, U, U, U, U, U, U, U, 		// 41-50
    U, U, U, U, U, U, U, U, U, U, 		// 51-60
    U, U, U, U, U, U, U,	 		// 61-67
    T::DO_IPOPT_TS, U, U,			// TS, 69-70
    U, U, U, U, U, U, U, U, U, U, 		// 71-80
    U, U, U, U, U, U, U, U, U, U, 		// 81-90
    U, U, U, U, U, U, U, U, U, U, 		// 91-100
    U, U, U, U, U, U, U, U, U, U, 		// 101-110
    U, U, U, U, U, U, U, U, U, U, 		// 111-120
    U, U, U, U, U, U, U, U, U,			// 121-129
    T::DO_IPOPT_UNKNOWN, T::DO_IPOPT_ROUTE,	// SECURITY, LSRR
    U, U, U, U,					// 132-135
    T::DO_IPOPT_UNKNOWN, T::DO_IPOPT_ROUTE, 	// SATID, SSRR
    U, U, U,					// 138-140
    U, U, U, U, U, U, U,	 		// 141-147
    T::DO_IPOPT_UNKNOWN				// RA
};

static int tcp_opt_contents_mapping[] = {
    T::DO_TCPOPT_PADDING, T::DO_TCPOPT_PADDING,	// EOL, NOP
    T::DO_TCPOPT_MSS, T::DO_TCPOPT_WSCALE,	// MAXSEG, WSCALE
    T::DO_TCPOPT_SACK, T::DO_TCPOPT_SACK,	// SACK_PERMITTED, SACK
    T::DO_TCPOPT_UNKNOWN, T::DO_TCPOPT_UNKNOWN,	// 6, 7
    T::DO_TCPOPT_TIMESTAMP			// TIMESTAMP
};
#undef T
#undef U

int
ToIPSummaryDump::store_ip_opt_binary(const uint8_t *opt, int opt_len, int contents, StringAccum &sa)
{
    if (contents == (int)DO_IPOPT_ALL) {
	// store all options
	sa.append((char)opt_len);
	sa.append(opt, opt_len);
	return opt_len + 1;
    }

    const uint8_t *end_opt = opt + opt_len;
    int initial_sa_len = sa.length();
    sa.append('\0');
    
    while (opt < end_opt) {
	// one-byte options
	if (*opt == IPOPT_EOL) {
	    if (contents & DO_IPOPT_PADDING)
		sa.append(opt, 1);
	    goto done;
	} else if (*opt == IPOPT_NOP) {
	    if (contents & DO_IPOPT_PADDING)
		sa.append(opt, 1);
	    opt++;
	    continue;
	}
	
	// quit copying options if you encounter something obviously invalid
	if (opt[1] < 2 || opt + opt[1] > end_opt)
	    break;

	int this_content = (*opt > IPOPT_RA ? (int)DO_IPOPT_UNKNOWN : ip_opt_contents_mapping[*opt]);
	if (contents & this_content)
	    sa.append(opt, opt[1]);
	opt += opt[1];
    }

  done:
    sa[initial_sa_len] = sa.length() - initial_sa_len - 1;
    return sa.length() - initial_sa_len;
}

inline int
ToIPSummaryDump::store_ip_opt_binary(const click_ip *iph, int contents, StringAccum &sa)
{
    return store_ip_opt_binary(reinterpret_cast<const uint8_t *>(iph + 1), (iph->ip_hl << 2) - sizeof(click_ip), contents, sa);
}

int
ToIPSummaryDump::store_tcp_opt_binary(const uint8_t *opt, int opt_len, int contents, StringAccum &sa)
{
    if (contents == (int)DO_TCPOPT_ALL) {
	// store all options
	sa.append((char)opt_len);
	sa.append(opt, opt_len);
	return opt_len + 1;
    }

    const uint8_t *end_opt = opt + opt_len;
    int initial_sa_len = sa.length();
    sa.append('\0');
    
    while (opt < end_opt) {
	// one-byte options
	if (*opt == TCPOPT_EOL) {
	    if (contents & DO_TCPOPT_PADDING)
		sa.append(opt, 1);
	    goto done;
	} else if (*opt == TCPOPT_NOP) {
	    if (contents & DO_TCPOPT_PADDING)
		sa.append(opt, 1);
	    opt++;
	    continue;
	}
	
	// quit copying options if you encounter something obviously invalid
	if (opt[1] < 2 || opt + opt[1] > end_opt)
	    break;

	int this_content = (*opt > TCPOPT_TIMESTAMP ? (int)DO_TCPOPT_UNKNOWN : tcp_opt_contents_mapping[*opt]);
	if (contents & this_content)
	    sa.append(opt, opt[1]);
	opt += opt[1];
    }

  done:
    sa[initial_sa_len] = sa.length() - initial_sa_len - 1;
    return sa.length() - initial_sa_len;
}

inline int
ToIPSummaryDump::store_tcp_opt_binary(const click_tcp *tcph, int contents, StringAccum &sa)
{
    return store_tcp_opt_binary(reinterpret_cast<const uint8_t *>(tcph + 1), (tcph->th_off << 2) - sizeof(click_tcp), contents, sa);
}

bool
ToIPSummaryDump::binary_summary(PacketDesc& d, StringAccum& sa, StringAccum* bad_sa) const
{
    assert(sa.length() == 0);
    char *buf = sa.extend(_binary_size);
    int pos = 4;
    
    // Print actual contents.
    for (int i = 0; i < _contents.size(); i++) {
	uint32_t v = 0, v2 = 0;

	bool ok = extract(d, _contents[i], v, v2, bad_sa);
	
	switch (_contents[i]) {
	  case W_NONE:
	    break;
	  case W_TIMESTAMP:
	  case W_FIRST_TIMESTAMP:
	  case W_TIMESTAMP_USEC1:
	    PUT4(buf + pos, v);
	    PUT4(buf + pos + 4, v2);
	    pos += 8;
	    break;
	  case W_TIMESTAMP_SEC:
	  case W_TIMESTAMP_USEC:
	  case W_TCP_SEQ:
	  case W_TCP_ACK:
	  case W_IP_LEN:
	  case W_PAYLOAD_LEN:
	  case W_IP_CAPTURE_LEN:
	  case W_COUNT:
	  case W_AGGREGATE:
	    PUT4(buf + pos, v);
	    pos += 4;
	    break;
	  case W_IP_SRC:
	  case W_IP_DST:
	    PUT4NET(buf + pos, ok ? v : 0);
	    pos += 4;
	    break;
	  case W_IP_TOS:
	  case W_IP_TTL:
	  case W_IP_FRAG:
	  case W_IP_PROTO:
	  case W_TCP_FLAGS:
	  case W_LINK:
	  output_1:
	    buf[pos] = v;
	    pos++;
	    break;
	  case W_IP_FRAGOFF:
	  case W_IP_ID:
	  case W_SPORT:
	  case W_DPORT:
	  case W_TCP_WINDOW:
	  case W_TCP_URP:
	    PUT2(buf + pos, v);
	    pos += 2;
	    break;
	  case W_IP_OPT: {
	      if (!ok || d.iph->ip_hl <= (sizeof(click_ip) >> 2))
		  goto output_1;
	      int left = sa.length() - pos;
	      sa.set_length(pos);
	      pos += store_ip_opt_binary(d.iph, DO_IPOPT_ALL, sa);
	      sa.extend(left);
	      buf = sa.data();
	      break;
	  }
	  case W_TCP_OPT: {
	      if (!ok || d.tcph->th_off <= (sizeof(click_tcp) >> 2))
		  goto output_1;
	      int left = sa.length() - pos;
	      sa.set_length(pos);
	      pos += store_tcp_opt_binary(d.tcph, DO_TCPOPT_ALL, sa);
	      sa.extend(left);
	      buf = sa.data();
	      break;
	  }
	  case W_TCP_NTOPT: {
	      if (!ok || d.tcph->th_off <= (sizeof(click_tcp) >> 2)
		  || (d.tcph->th_off == 8
		      && *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
		  goto output_1;
	      int left = sa.length() - pos;
	      sa.set_length(pos);
	      pos += store_tcp_opt_binary(d.tcph, DO_TCPOPT_NTALL, sa);
	      sa.extend(left);
	      buf = sa.data();
	      break;
	  }
	  case W_TCP_SACK: {
	      if (!ok || d.tcph->th_off <= (sizeof(click_tcp) >> 2)
		  || (d.tcph->th_off == 8
		      && *(reinterpret_cast<const uint32_t *>(d.tcph + 1)) == htonl(0x0101080A)))
		  goto output_1;
	      int left = sa.length() - pos;
	      sa.set_length(pos);
	      pos += store_tcp_opt_binary(d.tcph, DO_TCPOPT_SACK, sa);
	      sa.extend(left);
	      buf = sa.data();
	      break;
	  }
	}
    }

    sa.set_length(pos);
    *(reinterpret_cast<uint32_t *>(buf)) = htonl(pos);
    return true;
}

void
ToIPSummaryDump::write_packet(Packet* p, bool multipacket)
{
    if (multipacket && EXTRA_PACKETS_ANNO(p) > 0) {
	uint32_t count = 1 + EXTRA_PACKETS_ANNO(p);
	uint32_t total_len = p->length() + EXTRA_LENGTH_ANNO(p);
	uint32_t len = p->length();
	if (total_len < count * len)
	    total_len = count * len;

	// do timestamp stepping
	struct timeval end_timestamp = p->timestamp_anno();
	struct timeval timestamp_delta;
	if (timerisset(&FIRST_TIMESTAMP_ANNO(p))) {
	    timestamp_delta = (end_timestamp - FIRST_TIMESTAMP_ANNO(p)) / (count - 1);
	    p->set_timestamp_anno(FIRST_TIMESTAMP_ANNO(p));
	} else
	    timestamp_delta = make_timeval(0, 0);
	
	SET_EXTRA_PACKETS_ANNO(p, 0);
	for (uint32_t i = count; i > 0; i--) {
	    uint32_t l = total_len / i;
	    SET_EXTRA_LENGTH_ANNO(p, l - len);
	    total_len -= l;
	    write_packet(p, false);
	    if (i == 1)
		p->timestamp_anno() = end_timestamp;
	    else
		p->timestamp_anno() += timestamp_delta;
	}
	
    } else {
	_sa.clear();
	_bad_sa.clear();

	summary(p, _sa, (_bad_packets ? &_bad_sa : 0));

	if (_bad_packets && _bad_sa)
	    write_line(_bad_sa.take_string());
	fwrite(_sa.data(), 1, _sa.length(), _f);

	_output_count++;
    }
}

void
ToIPSummaryDump::push(int, Packet *p)
{
    if (_active)
	write_packet(p, _multipacket);
    p->kill();
}

bool
ToIPSummaryDump::run_task()
{
    if (!_active)
	return false;
    if (Packet *p = input(0).pull()) {
	write_packet(p, _multipacket);
	p->kill();
	_task.fast_reschedule();
	return true;
    } else if (_signal) {
	_task.fast_reschedule();
	return false;
    } else
	return false;
}

void
ToIPSummaryDump::write_line(const String& s)
{
    if (s.length()) {
	assert(s.back() == '\n');
	if (_binary) {
	    uint32_t marker = htonl(s.length() | 0x80000000U);
	    fwrite(&marker, 4, 1, _f);
	}
	fwrite(s.data(), 1, s.length(), _f);
    }
}

void
ToIPSummaryDump::add_note(const String &s)
{
    if (s.length()) {
	int extra = 1 + (s.back() == '\n' ? 0 : 1);
	if (_binary) {
	    uint32_t marker = htonl((s.length() + extra) | 0x80000000U);
	    fwrite(&marker, 4, 1, _f);
	}
	fputc('#', _f);
	fwrite(s.data(), 1, s.length(), _f);
	if (extra > 1)
	    fputc('\n', _f);
    }
}

void
ToIPSummaryDump::flush_buffer()
{
    fflush(_f);
}

void
ToIPSummaryDump::add_handlers()
{
    if (input_is_pull(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel IPSummaryDumpInfo)
EXPORT_ELEMENT(ToIPSummaryDump)
CLICK_ENDDECLS
