/*
 * ipfilter.{cc,hh} -- IP-packet filter with tcpdumplike syntax
 * Eddie Kohler
 *
 * Copyright (c) 2000-2001 Mazu Networks, Inc.
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
#include "ipfilter.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/icmp.h>
#include <click/hashmap.hh>
CLICK_DECLS

static HashMap<String, int> *wordmap;
static int ip_filter_count;
#define WT(t, d)		((t << WT_TYPE_SHIFT) | d)
#define WT_TYPE(wt)		((wt & IPFilter::WT_TYPE_MASK) >> IPFilter::WT_TYPE_SHIFT)

HashMap<String, int> *
IPFilter::create_wordmap()
{
  HashMap<String, int> *wordmap = new HashMap<String, int>(0);

  wordmap->insert("host",	WT(TYPE_TYPE, TYPE_HOST));
  wordmap->insert("net",	WT(TYPE_TYPE, TYPE_NET));
  wordmap->insert("port",	WT(TYPE_TYPE, TYPE_PORT));
  wordmap->insert("proto",	WT(TYPE_TYPE, TYPE_PROTO));
  wordmap->insert("opt",	WT(TYPE_TYPE, TYPE_TCPOPT));
  wordmap->insert("tos",	WT(TYPE_TYPE, TYPE_TOS));
  wordmap->insert("ttl",	WT(TYPE_TYPE, TYPE_TTL));
  wordmap->insert("dscp",	WT(TYPE_TYPE, TYPE_DSCP));
  wordmap->insert("type",	WT(TYPE_TYPE, TYPE_ICMP_TYPE));
  wordmap->insert("frag",	WT(TYPE_TYPE, TYPE_IPFRAG));
  wordmap->insert("unfrag",	WT(TYPE_TYPE, TYPE_IPUNFRAG));
  wordmap->insert("ect",	WT(TYPE_TYPE, TYPE_IPECT));
  wordmap->insert("ce",		WT(TYPE_TYPE, TYPE_IPCE));
  wordmap->insert("len",	WT(TYPE_TYPE, TYPE_IPLEN));
  
  wordmap->insert("icmp",	WT(TYPE_PROTO, IP_PROTO_ICMP));
  wordmap->insert("igmp",	WT(TYPE_PROTO, IP_PROTO_IGMP));
  wordmap->insert("ipip",	WT(TYPE_PROTO, IP_PROTO_IPIP));
  wordmap->insert("tcp",	WT(TYPE_PROTO, IP_PROTO_TCP));
  wordmap->insert("udp",	WT(TYPE_PROTO, IP_PROTO_UDP));
  wordmap->insert("tcpudp",	WT(TYPE_PROTO, IP_PROTO_TCP_OR_UDP));
  
  wordmap->insert("echo",	WT(TYPE_PORT, 7));
  wordmap->insert("discard",	WT(TYPE_PORT, 9));
  wordmap->insert("daytime",	WT(TYPE_PORT, 13));
  wordmap->insert("chargen",	WT(TYPE_PORT, 19));
  wordmap->insert("ftp-data",	WT(TYPE_PORT, 20));
  wordmap->insert("ftp",	WT(TYPE_PORT, 21));
  wordmap->insert("ssh",	WT(TYPE_PORT, 22));
  wordmap->insert("telnet",	WT(TYPE_PORT, 23));
  wordmap->insert("smtp",	WT(TYPE_PORT, 25));
  wordmap->insert("domain",	WT(TYPE_PORT, 53));
  wordmap->insert("dns",	WT(TYPE_PORT, 53));
  wordmap->insert("bootps",	WT(TYPE_PORT, 67));
  wordmap->insert("bootpc",	WT(TYPE_PORT, 68));
  wordmap->insert("tftp",	WT(TYPE_PORT, 69));
  wordmap->insert("finger",	WT(TYPE_PORT, 79));
  wordmap->insert("www",	WT(TYPE_PORT, 80));
  wordmap->insert("pop3",	WT(TYPE_PORT, 110));
  wordmap->insert("sunrpc",	WT(TYPE_PORT, 111));
  wordmap->insert("auth",	WT(TYPE_PORT, 113));
  wordmap->insert("nntp",	WT(TYPE_PORT, 119));
  wordmap->insert("ntp",	WT(TYPE_PORT, 123));
  wordmap->insert("netbios-ns",	WT(TYPE_PORT, 137));
  wordmap->insert("netbios-dgm",WT(TYPE_PORT, 138));
  wordmap->insert("netbios-ssn",WT(TYPE_PORT, 139));
  wordmap->insert("snmp",	WT(TYPE_PORT, 161));
  wordmap->insert("snmp-trap",	WT(TYPE_PORT, 162));
  wordmap->insert("irc",	WT(TYPE_PORT, 194));
  wordmap->insert("imap3",	WT(TYPE_PORT, 220));
  wordmap->insert("https",	WT(TYPE_PORT, 443));
  wordmap->insert("rip",	WT(TYPE_PORT, 520));
  wordmap->insert("route",	WT(TYPE_PORT, 520));
  wordmap->insert("imaps",	WT(TYPE_PORT, 993));
  wordmap->insert("pop3s",	WT(TYPE_PORT, 995));

  wordmap->insert("syn",	WT(TYPE_TCPOPT, TH_SYN));
  wordmap->insert("fin",	WT(TYPE_TCPOPT, TH_FIN));
  wordmap->insert("ack",	WT(TYPE_TCPOPT, TH_ACK));
  wordmap->insert("rst",	WT(TYPE_TCPOPT, TH_RST));
  wordmap->insert("psh",	WT(TYPE_TCPOPT, TH_PUSH));
  wordmap->insert("urg",	WT(TYPE_TCPOPT, TH_URG));

  wordmap->insert("echo-reply",	WT(TYPE_ICMP_TYPE, ICMP_ECHOREPLY));
  wordmap->insert("unreachable", WT(TYPE_ICMP_TYPE, ICMP_UNREACH));
  wordmap->insert("sourcequench", WT(TYPE_ICMP_TYPE, ICMP_SOURCEQUENCH));
  wordmap->insert("redirect",	WT(TYPE_ICMP_TYPE, ICMP_REDIRECT));
  wordmap->insert("echo+",	WT(TYPE_ICMP_TYPE, ICMP_ECHO));
  wordmap->insert("routeradvert", WT(TYPE_ICMP_TYPE, ICMP_ROUTERADVERT));
  wordmap->insert("routersolicit", WT(TYPE_ICMP_TYPE, ICMP_ROUTERSOLICIT));
  wordmap->insert("timeexceeded", WT(TYPE_ICMP_TYPE, ICMP_TIMXCEED));
  wordmap->insert("parameterproblem", WT(TYPE_ICMP_TYPE, ICMP_PARAMPROB));
  wordmap->insert("timestamp",	WT(TYPE_ICMP_TYPE, ICMP_TSTAMP));
  wordmap->insert("timestamp-reply", WT(TYPE_ICMP_TYPE, ICMP_TSTAMPREPLY));
  wordmap->insert("inforeq",	WT(TYPE_ICMP_TYPE, ICMP_IREQ));
  wordmap->insert("inforeq-reply", WT(TYPE_ICMP_TYPE, ICMP_IREQREPLY));
  wordmap->insert("maskreq",	WT(TYPE_ICMP_TYPE, ICMP_MASKREQ));
  wordmap->insert("maskreq-reply", WT(TYPE_ICMP_TYPE, ICMP_MASKREQREPLY));

  // deprecated variants
  wordmap->insert("echo_reply",	WT(TYPE_ICMP_TYPE, ICMP_ECHOREPLY));
  wordmap->insert("dst_unreachable", WT(TYPE_ICMP_TYPE, ICMP_UNREACH));
  wordmap->insert("source_quench", WT(TYPE_ICMP_TYPE, ICMP_SOURCEQUENCH));
  wordmap->insert("time_exceeded", WT(TYPE_ICMP_TYPE, ICMP_TIMXCEED));
  wordmap->insert("parameter_problem", WT(TYPE_ICMP_TYPE, ICMP_PARAMPROB));
  wordmap->insert("time_stamp", WT(TYPE_ICMP_TYPE, ICMP_TSTAMP));
  wordmap->insert("time_stamp_reply", WT(TYPE_ICMP_TYPE, ICMP_TSTAMPREPLY));
  wordmap->insert("info_request", WT(TYPE_ICMP_TYPE, ICMP_IREQ));
  wordmap->insert("info_request_reply", WT(TYPE_ICMP_TYPE, ICMP_IREQREPLY));

  return wordmap;
}

static void
accum_wt_names(HashMap<String, int> *wordmap, String word, StringAccum &sa)
{
  int t = (*wordmap)[word];
  Vector<int> types;
  while (t != 0) {
    types.push_back(WT_TYPE(t));
    word += "+";
    t = (*wordmap)[word];
  }

  if (types.size() == 0)
    /* nada */;
  else if (types.size() == 1)
    sa << '`' << IPFilter::Primitive::unparse_type(0, types[0]) << '\'';
  else if (types.size() == 2)
    sa << '`' << IPFilter::Primitive::unparse_type(0, types[0]) << "\' or `" << IPFilter::Primitive::unparse_type(0, types[1]) << '\'';
  else {
    for (int i = 0; i < types.size() - 1; i++)
      sa << '`' << IPFilter::Primitive::unparse_type(0, types[i]) << "\', ";
    sa << "or `" << IPFilter::Primitive::unparse_type(0, types.back()) << '\'';
  }
}

int
IPFilter::lookup_word(HashMap<String, int> *wordmap, int type, int transp_proto, String word, ErrorHandler *errh)
{
  String orig_word = word;
  int t = (*wordmap)[word];
  if (t == 0)
    return -1;

  int num_matches = 0;
  int last_match = -1;
  int transp_dropped = 0;

  while (t != 0) {
    if (type != 0 && WT_TYPE(t) != type)
      /* not interesting */;
    else if (WT_TYPE(t) == TYPE_TCPOPT && transp_proto != UNKNOWN && transp_proto != IP_PROTO_TCP && transp_proto != IP_PROTO_TCP_OR_UDP) {
      /* not interesting */
      transp_dropped++;
    } else if (WT_TYPE(t) == TYPE_ICMP_TYPE && transp_proto != UNKNOWN && transp_proto != IP_PROTO_ICMP) {
      /* not interesting */
      transp_dropped++;
    } else if (WT_TYPE(t) == TYPE_PORT && transp_proto != UNKNOWN && transp_proto != IP_PROTO_TCP && transp_proto != IP_PROTO_UDP && transp_proto != IP_PROTO_TCP_OR_UDP) {
      /* not interesting */
      transp_dropped++;
    } else {
      num_matches++;
      last_match = t;
    }

    word += "+";
    t = (*wordmap)[word];
  }

  if (num_matches == 1)
    return last_match;
  else if (num_matches > 1) {
    if (errh) {
      StringAccum sa;
      accum_wt_names(wordmap, orig_word, sa);
      errh->error("`%s' is ambiguous; specify %s", orig_word.cc(), sa.cc());
    }
    return -2;
  } else {
    String tn = Primitive::unparse_type(0, type);
    String tr = Primitive::unparse_transp_proto(transp_proto);
    if (tr) tr += " ";
    StringAccum sa;
    accum_wt_names(wordmap, orig_word, sa);
    errh->error("`%s%s %s' is meaningless; specify %s with %s", tr.cc(), tn.cc(), orig_word.cc(), sa.cc(), orig_word.cc());
    return -2;
  }
}

IPFilter::IPFilter()
{
  // no MOD_INC_USE_COUNT; rely on Classifier
  if (ip_filter_count == 0)
    wordmap = create_wordmap();
  else
    assert(wordmap);
  ip_filter_count++;
}

IPFilter::~IPFilter()
{
  // no MOD_DEC_USE_COUNT; rely on Classifier
  ip_filter_count--;
  if (ip_filter_count == 0) {
    delete wordmap;
    wordmap = 0;
  }
}

IPFilter *
IPFilter::clone() const
{
  return new IPFilter;
}

//
// CONFIGURATION
//

void
IPFilter::Primitive::clear()
{
  _type = _srcdst = 0;
  _transp_proto = UNKNOWN;
  _data = 0;
  _op = OP_EQ;
  _op_negated = false;
}

void
IPFilter::Primitive::set_type(int x, ErrorHandler *errh)
{
  if (_type)
    errh->error("type specified twice");
  _type = x;
}

void
IPFilter::Primitive::set_srcdst(int x, ErrorHandler *errh)
{
  if (_srcdst)
    errh->error("`src' or `dst' specified twice");
  _srcdst = x;
}

void
IPFilter::Primitive::set_transp_proto(int x, ErrorHandler *errh)
{
  if (_transp_proto != UNKNOWN)
    errh->error("transport protocol specified twice");
  _transp_proto = x;
}

int
IPFilter::Primitive::set_mask(uint32_t full_mask, int shift, ErrorHandler *errh)
{
  uint32_t data = _u.u;

  // Two kinds of GT or LT tests are OK: those that boil down to checking
  // the upper bits against 0, and those that boil down to checking the upper
  // bits against ~0.
  if (_op == OP_GT || _op == OP_LT) {
    // Check for power of 2 -- compare upper bits against 0.
    uint32_t data_x = (_op == OP_GT ? data + 1 : data);
    if ((data_x & (data_x - 1)) == 0 && (data_x - 1) <= full_mask && data_x <= full_mask) {
      _u.u = 0;
      _mask.u = (full_mask & ~(data_x - 1)) << shift;
      if (_op == OP_GT)
	_op_negated = !_op_negated;
      return 0;
    }

    // Check for comparisons that are always true.
    if ((_op == OP_LT && data == 0 && _op_negated)
	|| (_op == OP_LT && data > full_mask && !_op_negated)
	|| (_op == OP_GT && data >= full_mask && _op_negated)) {
      if (data)
	errh->warning("relation `%s %u' is always true (range 0-%u)", unparse_op().cc(), data, full_mask);
      _u.u = _mask.u = 0;
      _op_negated = false;
      return 0;
    }

    // Check for comparisons that are always false.
    if ((_op == OP_LT && (data == 0 || data > full_mask))
	|| (_op == OP_GT && data >= full_mask)) {
      errh->warning("relation `%s %u' is always false (range 0-%u)", unparse_op().cc(), data, full_mask);
      _u.u = _mask.u = 0;
      _op_negated = true;
      return 0;
    }

    // Check for (full_mask - [power of 2]) -- compare upper bits against ~0.
    // 8-bit field: can do > 191/<= 191, >= 192/< 192.
    uint32_t data_x_comp = (~data_x & full_mask) + 1;
    if ((data_x_comp & (data_x_comp - 1)) == 0 && data_x <= full_mask) {
      _u.u = data_x << shift;
      _mask.u = (full_mask & data_x) << shift;
      if (_op == OP_LT)
	_op_negated = !_op_negated;
      return 0;
    }

    // Remaining possibility is too-complex expression; print helpful message.
    uint32_t below_x_1 = 0, above_x_1 = 0, below_x_2 = 0, above_x_2 = 0;
    for (int i = 31; i >= 0; i--)
      if (data_x & (1 << i)) {
	below_x_1 = 1 << i;
	above_x_1 = below_x_1 << 1;
	if (above_x_1 > full_mask || above_x_1 == 0)
	  for (int j = i - 1; j >= 0; j--)
	    if (!(data_x & (1 << j))) {
	      below_x_2 = data_x & ~((2 << j) - 1);
	      above_x_2 = below_x_2 | (1 << j);
	      break;
	    }
	break;
      }
    uint32_t below_x = (below_x_2 && (data_x - below_x_2 < data_x - below_x_1) ? below_x_2 : below_x_1) - (_op == OP_GT ? 1 : 0);
    uint32_t above_x = (above_x_2 && (above_x_2 - data_x < above_x_1 - data_x) ? above_x_2 : above_x_1) - (_op == OP_GT ? 1 : 0);
    return errh->error("relation `%s %u' too hard\n(Closest possibilities are `%s %u' and `%s %u'.)", unparse_op().cc(), data, unparse_op().cc(), below_x, unparse_op().cc(), above_x);
  }

  if (data > full_mask)
    return errh->error("value %u out of range (0-%u)", data, full_mask);

  _u.u = data << shift;
  _mask.u = full_mask << shift;
  return 0;
}

String
IPFilter::Primitive::unparse_type(int srcdst, int type)
{
  StringAccum sa;
  
  switch (srcdst) {
   case SD_SRC: sa << "src "; break;
   case SD_DST: sa << "dst "; break;
   case SD_OR: sa << "src or dst "; break;
   case SD_AND: sa << "src and dst "; break;
  }

  switch (type) {
   case TYPE_NONE: sa << "<none> "; break;
   case TYPE_HOST: sa << "ip host "; break;
   case TYPE_PROTO: sa << "proto "; break;
   case TYPE_TOS: sa << "ip tos "; break;
   case TYPE_TTL: sa << "ip ttl "; break;
   case TYPE_IPFRAG: sa << "ip frag "; break;
   case TYPE_IPLEN: sa << "ip len "; break;
   case TYPE_PORT: sa << "port "; break;
   case TYPE_TCPOPT: sa << "tcp opt "; break;
   case TYPE_ICMP_TYPE: sa << "icmp type "; break;
   case TYPE_NET: sa << "ip net "; break;
   case TYPE_DSCP: sa << "ip dscp "; break;
   case TYPE_IPUNFRAG: sa << "ip unfrag "; break;
   case TYPE_IPECT: sa << "ip ect "; break;
   case TYPE_IPCE: sa << "ip ce "; break;
   default: sa << "<unknown type " << type << "> "; break;
  }

  sa.pop_back();
  return sa.take_string();
}

String
IPFilter::Primitive::unparse_transp_proto(int transp_proto)
{
  switch (transp_proto) {
   case UNKNOWN: return "";
   case IP_PROTO_ICMP: return "icmp";
   case IP_PROTO_IGMP: return "igmp";
   case IP_PROTO_IPIP: return "ipip";
   case IP_PROTO_TCP: return "tcp";
   case IP_PROTO_UDP: return "udp";
   case IP_PROTO_TCP_OR_UDP: return "tcpudp";
   default: return "ip proto " + String(transp_proto);
  }
}

String
IPFilter::Primitive::unparse_type() const
{
  return unparse_type(_srcdst, _type);
}

String
IPFilter::Primitive::unparse_op() const
{
  if (_op == OP_GT)
    return (_op_negated ? "<=" : ">");
  else if (_op == OP_LT)
    return (_op_negated ? ">=" : "<");
  else
    return (_op_negated ? "!=" : "=");
}

void
IPFilter::Primitive::simple_negate()
{
  assert(negation_is_simple());
  _op_negated = !_op_negated;
  if (_type == TYPE_PROTO && _mask.u == 0xFF)
    _transp_proto = (_op_negated ? UNKNOWN : _u.i);
}

int
IPFilter::Primitive::check(const Primitive &p, ErrorHandler *errh)
{
  int old_srcdst = _srcdst;

  // if _type is erroneous, return -1 right away
  if (_type < 0)
    return -1;
  
  // set _type if it was not specified
  if (!_type) {

   retry:
    switch (_data) {

     case TYPE_HOST:
     case TYPE_NET:
     case TYPE_TCPOPT:
     case TYPE_ICMP_TYPE:
      _type = _data;
      if (!_srcdst) _srcdst = p._srcdst;
      break;

     case TYPE_PROTO:
      _type = TYPE_PROTO;
      break;
      
     case TYPE_PORT:
      _type = TYPE_PORT;
      if (!_srcdst) _srcdst = p._srcdst;
      if (_transp_proto == UNKNOWN) _transp_proto = p._transp_proto;
      break;
      
     case TYPE_INT:
      if (p._type != TYPE_PROTO && p._type != TYPE_PORT && p._type != TYPE_ICMP_TYPE && p._type != TYPE_IPLEN)
	return errh->error("specify `proto', `port', `icmp type', or `ip len'");
      _data = p._type;
      goto retry;

     case TYPE_NONE:
      if (_transp_proto != UNKNOWN)
	_type = TYPE_PROTO;
      else
	return errh->error("partial directive");
      break;
      
     default:
      return errh->error("unknown type `%s'", unparse_type(0, _data).cc());

    }
  }

  // check that _data and _type agree
  switch (_type) {

   case TYPE_HOST:
    if (_data != TYPE_HOST)
      return errh->error("IP address missing in `host' directive");
    if (_op != OP_EQ)
      return errh->error("can't use relational operators with `host'");
    _mask.u = 0xFFFFFFFFU;
    break;

   case TYPE_NET:
    if (_data != TYPE_NET)
      return errh->error("IP prefix missing in `net' directive");
    if (_op != OP_EQ)
      return errh->error("can't use relational operators with `net'");
    _type = TYPE_HOST;
    // _mask already set
    break;

   case TYPE_PROTO:
    if (_data == TYPE_INT || _data == TYPE_PROTO) {
      if (_transp_proto != UNKNOWN)
	return errh->error("transport protocol specified twice");
      _data = TYPE_NONE;
    } else
      _u.i = _transp_proto;
    _transp_proto = UNKNOWN;
    if (_data != TYPE_NONE || _u.i == UNKNOWN)
      return errh->error("IP protocol missing in `proto' directive");
    if (_u.i == IP_PROTO_TCP_OR_UDP) {
      if (_op != OP_EQ)
	return errh->error("can't use relational operators with `tcpudp'");
      _mask.u = 0xFF;
    } else if (set_mask(0xFF, 0, errh) < 0)
      return -1;
    if (_mask.u == 0xFF && !_op_negated) // set _transp_proto if allowed
      _transp_proto = _u.i;
    break;

   case TYPE_PORT:
    if (_data == TYPE_INT)
      _data = TYPE_PORT;
    if (_data != TYPE_PORT)
      return errh->error("port number missing in `port' directive");
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_TCP_OR_UDP;
    else if (_transp_proto != IP_PROTO_TCP && _transp_proto != IP_PROTO_UDP && _transp_proto != IP_PROTO_TCP_OR_UDP)
      return errh->error("bad protocol %d for `port' directive", _transp_proto);
    if (set_mask(0xFFFF, 0, errh) < 0)
      return -1;
    break;

   case TYPE_TCPOPT:
    if (_data == TYPE_INT)
      _data = TYPE_TCPOPT;
    if (_data != TYPE_TCPOPT)
      return errh->error("TCP options missing in `tcp opt' directive");
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_TCP;
    else if (_transp_proto != IP_PROTO_TCP)
      return errh->error("bad protocol %d for `tcp opt' directive", _transp_proto);
    if (_op != OP_EQ || _op_negated)
      return errh->error("can't use relational operators with `tcp opt'");
    if (_u.i < 0 || _u.i > 255)
      return errh->error("value %d out of range", _u.i);
    _mask.i = _u.i;
    break;

   case TYPE_TOS:
    if (_data != TYPE_INT)
      return errh->error("TOS value missing in `ip tos' directive");
    if (set_mask(0xFF, 0, errh) < 0)
      return -1;
    break;

   case TYPE_DSCP:
    if (_data != TYPE_INT)
      return errh->error("DSCP missing in `ip dscp' directive");
    if (set_mask(0x3F, 2, errh) < 0)
      return -1;
    _type = TYPE_TOS;
    break;

   case TYPE_IPECT:
    if (_data == TYPE_NONE) {
      _mask.u = IP_ECNMASK;
      _u.u = 0;
      _op_negated = true;
    } else if (_data == TYPE_INT) {
      if (set_mask(0x3, 0, errh) < 0)
	return -1;
    } else
      return errh->error("weird data given to `ip ect' directive");
    _type = TYPE_TOS;
    break;

   case TYPE_IPCE:
    if (_data != TYPE_NONE)
      return errh->error("`ip ce' directive takes no data");
    _type = TYPE_TOS;
    _mask.u = IP_ECNMASK;
    _u.u = IP_ECN_CE;
    break;

   case TYPE_TTL:
    if (_data != TYPE_INT)
      return errh->error("TTL value missing in `ip ttl' directive");
    if (set_mask(0xFF, 0, errh) < 0)
      return -1;
    break;

   case TYPE_IPLEN:
    if (_data != TYPE_INT)
      return errh->error("length value missing in `ip len' directive");
    if (set_mask(0xFFFF, 0, errh) < 0)
      return -1;
    break;

   case TYPE_ICMP_TYPE:
    if (_data == TYPE_INT)
      _data = TYPE_ICMP_TYPE;
    if (_data != TYPE_ICMP_TYPE)
      return errh->error("ICMP type missing in `icmp type' directive");
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_ICMP;
    else if (_transp_proto != IP_PROTO_ICMP)
      return errh->error("bad protocol %d for `icmp type' directive", _transp_proto);
    if (set_mask(0xFF, 0, errh) < 0)
      return -1;
    break;

   case TYPE_IPFRAG:
    if (_data != TYPE_NONE)
      return errh->error("`ip frag' directive takes no data");
    break;

   case TYPE_IPUNFRAG:
    if (_data != TYPE_NONE)
      return errh->error("`ip unfrag' directive takes no data");
    _op_negated = true;
    _type = TYPE_IPFRAG;
    break;
    
  }

  // fix _srcdst
  if (_type == TYPE_HOST || _type == TYPE_PORT) {
    if (_srcdst == 0)
      _srcdst = SD_OR;
  } else if (old_srcdst)
    errh->warning("`src' or `dst' is meaningless here");
  
  return 0;
}

static void
add_exprs_for_proto(int32_t proto, int32_t mask, Classifier *c, Vector<int> &tree)
{
  if (mask == 0xFF && proto == IP_PROTO_TCP_OR_UDP) {
    c->start_expr_subtree(tree);
    c->add_expr(tree, 8, htonl(IP_PROTO_TCP << 16), htonl(0x00FF0000));
    c->add_expr(tree, 8, htonl(IP_PROTO_UDP << 16), htonl(0x00FF0000));
    c->finish_expr_subtree(tree, false);
  } else
    c->add_expr(tree, 8, htonl(proto << 16), htonl(mask << 16));
}

void
IPFilter::Primitive::add_exprs(Classifier *c, Vector<int> &tree) const
{
  Expr e;

  c->start_expr_subtree(tree);

  // enforce first fragment: fragmentation offset == 0
  // (before transport protocol to enhance later optimizations)
  if (_type == TYPE_PORT || _type == TYPE_TCPOPT || _type == TYPE_ICMP_TYPE)
    c->add_expr(tree, 4, 0, htonl(0x00001FFF));
  
  // handle transport protocol uniformly
  if (_type == TYPE_PROTO)
    add_exprs_for_proto(_u.i, _mask.i, c, tree);
  else if (_transp_proto != UNKNOWN)
    add_exprs_for_proto(_transp_proto, 0xFF, c, tree);

  // handle other types
  switch (_type) {

   case TYPE_HOST: {
     e.mask.u = _mask.u;
     e.value.u = _u.u & _mask.u;
     c->start_expr_subtree(tree);
     if (_srcdst == SD_SRC || _srcdst == SD_AND || _srcdst == SD_OR) {
       c->add_expr(tree, 12, e.value.u, e.mask.u);
       if (_op_negated) c->negate_expr_subtree(tree);
     }
     if (_srcdst == SD_DST || _srcdst == SD_AND || _srcdst == SD_OR) {
       c->add_expr(tree, 16, e.value.u, e.mask.u);
       if (_op_negated) c->negate_expr_subtree(tree);
     }
     c->finish_expr_subtree(tree, _srcdst != SD_OR);
     break;
   }

   case TYPE_PROTO:
    // do nothing
    break;

   case TYPE_TOS: {
     c->start_expr_subtree(tree);
     c->add_expr(tree, 0, htonl(_u.u << 16), htonl(_mask.u << 16));
     c->finish_expr_subtree(tree, true);
     if (_op_negated) c->negate_expr_subtree(tree);
     break;
   }

   case TYPE_TTL: {
     c->start_expr_subtree(tree);
     c->add_expr(tree, 8, htonl(_u.u << 24), htonl(_mask.u << 24));
     c->finish_expr_subtree(tree, true);
     if (_op_negated) c->negate_expr_subtree(tree);
     break;
   }

   case TYPE_IPFRAG: {
     c->start_expr_subtree(tree);
     c->add_expr(tree, 4, 0, htonl(0x00003FFF));
     c->finish_expr_subtree(tree, true);
     if (!_op_negated) c->negate_expr_subtree(tree);
     break;
   }

   case TYPE_IPLEN: {
     c->start_expr_subtree(tree);
     c->add_expr(tree, 0, htonl(_u.u), htonl(_mask.u));
     c->finish_expr_subtree(tree, true);
     if (_op_negated) c->negate_expr_subtree(tree);
     break;
   }

   case TYPE_PORT: {
     uint32_t mask = (htons(_mask.u) << 16) | htons(_mask.u);
     uint32_t ports = (htons(_u.u) << 16) | htons(_u.u);
    
     c->start_expr_subtree(tree);
     if (_srcdst == SD_SRC || _srcdst == SD_AND || _srcdst == SD_OR) {
       c->add_expr(tree, TRANSP_FAKE_OFFSET, ports, mask & htonl(0xFFFF0000));
       if (_op_negated) c->negate_expr_subtree(tree);
     }
     if (_srcdst == SD_DST || _srcdst == SD_AND || _srcdst == SD_OR) {
       c->add_expr(tree, TRANSP_FAKE_OFFSET, ports, mask & htonl(0x0000FFFF));
       if (_op_negated) c->negate_expr_subtree(tree);
     }
     c->finish_expr_subtree(tree, _srcdst != SD_OR);
     break;
   }

   case TYPE_TCPOPT: {
     c->add_expr(tree, TRANSP_FAKE_OFFSET + 12, htonl(_u.u << 16), htonl(_mask.u << 16));
     break;
   }

   case TYPE_ICMP_TYPE: {
     c->start_expr_subtree(tree);
     c->add_expr(tree, TRANSP_FAKE_OFFSET, htonl(_u.u << 24), htonl(_mask.u << 24));
     c->finish_expr_subtree(tree, true);
     if (_op_negated) c->negate_expr_subtree(tree);
     break;
   }

   default:
    assert(0);

  }

  c->finish_expr_subtree(tree, true);
}


static void
separate_words(Vector<String> &words)
{
  Vector<String> out;
  for (int i = 0; i < words.size(); i++) {
    const char *data = words[i].data();
    int len = words[i].length();
    int first = 0;
    for (int pos = 0; pos < len; pos++) {
      int length = -1;
      if (data[pos] == '(' || data[pos] == ')' || data[pos] == '!')
	length = 1;
      else if (data[pos] == '&' || data[pos] == '|')
	length = (pos < len-1 && data[pos+1] == data[pos] ? 2 : 1);
      else if (data[pos] == '<' || data[pos] == '>')
	length = (pos < len-1 && data[pos+1] == '=' ? 2 : 1);
      if (length > 0) {
	if (pos != first)
	  out.push_back(words[i].substring(first, pos - first));
	out.push_back(words[i].substring(pos, length));
	first = pos + length;
	pos += length - 1;
      }
    }
    if (len != first)
      out.push_back(words[i].substring(first, len - first));
  }
  words = out;
}

void
IPFilter::notify_noutputs(int n)
{
  set_noutputs(n);
}

/*
 * expr ::= expr || expr
 *	|   expr or expr
 *	|   term
 *	;
 * term ::= term && term
 *	|   term and term
 *	|   term factor			// juxtaposition = and
 *	|   term
 * factor ::= ! factor
 *	|   true
 *	|   false
 *	|   quals data
 *	|   quals relop data
 *	|   ( expr )
 *	;
 */

int
IPFilter::parse_expr(const Vector<String> &words, int pos,
		     Vector<int> &tree, Primitive &prev_prim,
		     ErrorHandler *errh)
{
  start_expr_subtree(tree);

  while (1) {
    pos = parse_term(words, pos, tree, prev_prim, errh);
    if (pos >= words.size())
      break;
    if (words[pos] == "or" || words[pos] == "||")
      pos++;
    else
      break;
  }

  finish_expr_subtree(tree, false);
  return pos;
}

int
IPFilter::parse_term(const Vector<String> &words, int pos,
		     Vector<int> &tree, Primitive &prev_prim,
		     ErrorHandler *errh)
{
  start_expr_subtree(tree);

  bool blank_ok = false;
  while (1) {
    int next = parse_factor(words, pos, tree, prev_prim, false, errh);
    if (next == pos)
      break;
    blank_ok = true;
    if (next < words.size() && (words[next] == "and" || words[next] == "&&")) {
      blank_ok = false;
      next++;
    }
    pos = next;
  }

  if (!blank_ok)
    errh->error("missing term");
  finish_expr_subtree(tree, true);
  return pos;
}

int
IPFilter::parse_factor(const Vector<String> &words, int pos,
		       Vector<int> &tree, Primitive &prev_prim,
		       bool negated, ErrorHandler *errh)
{
  // return immediately on last word, ")", "||", "or"
  if (pos >= words.size() || words[pos] == ")" || words[pos] == "||" || words[pos] == "or")
    return pos;

  // easy cases
  
  // `true' and `false'
  if (words[pos] == "true") {
    add_expr(tree, 0, 0, 0);
    if (negated)
      negate_expr_subtree(tree);
    return pos + 1;
  }
  if (words[pos] == "false") {
    add_expr(tree, 0, 0, 0);
    if (!negated)
      negate_expr_subtree(tree);
    return pos + 1;
  }
  // ! factor
  if (words[pos] == "not" || words[pos] == "!") {
    int next = parse_factor(words, pos + 1, tree, prev_prim, !negated, errh);
    if (next == pos + 1)
      errh->error("missing factor after `%s'", String(words[pos]).cc());
    return next;
  }
  // ( expr )
  if (words[pos] == "(") {
    int next = parse_expr(words, pos + 1, tree, prev_prim, errh);
    if (next == pos + 1)
      errh->error("missing expression after `('");
    if (next >= 0) {
      if (next >= words.size() || words[next] != ")")
	errh->error("missing `)'");
      else
	next++;
      if (negated)
	negate_expr_subtree(tree);
    }
    return next;
  }

  // hard case
  
  // expect quals [relop] data
  int first_pos = pos;
  Primitive prim;

  // collect qualifiers
  for (; pos < words.size(); pos++) {
    String wd = words[pos];
    int wt = lookup_word(wordmap, 0, UNKNOWN, wd, 0);

    if (wt >= 0 && WT_TYPE(wt) == TYPE_TYPE)
      prim.set_type(wt & WT_DATA, errh);

    else if (wt >= 0 && WT_TYPE(wt) == TYPE_PROTO)
      prim.set_transp_proto(wt & WT_DATA, errh);

    else if (wt != -1)
      break;

    else if (wd == "src") {
      if (pos < words.size() - 2 && (words[pos+2] == "dst" || words[pos+2] == "dest")) {
	if (words[pos+1] == "and" || words[pos+1] == "&&") {
	  prim.set_srcdst(SD_AND, errh);
	  pos += 2;
	} else if (words[pos+1] == "or" || words[pos+1] == "||") {
	  prim.set_srcdst(SD_OR, errh);
	  pos += 2;
	} else
	  prim.set_srcdst(SD_SRC, errh);
      } else
	prim.set_srcdst(SD_SRC, errh);
    } else if (wd == "dst" || wd == "dest")
      prim.set_srcdst(SD_DST, errh);
    
    else if (wd == "ip")
      /* nada */;
    
    else if (wd == "not" || wd == "!")
      negated = !negated;
      
    else
      break;
  }

  // prev_prim is not relevant if there were any qualifiers
  if (pos != first_pos)
    prev_prim.clear();
  
  // optional relational operation
  String wd = (pos >= words.size() - 1 ? String() : words[pos]);
  pos++;
  if (wd == "=" || wd == "==")
    /* nada */;
  else if (wd == "!=")
    prim._op_negated = true;
  else if (wd == ">")
    prim._op = OP_GT;
  else if (wd == "<")
    prim._op = OP_LT;
  else if (wd == ">=") {
    prim._op = OP_LT;
    prim._op_negated = true;
  } else if (wd == "<=") {
    prim._op = OP_GT;
    prim._op_negated = true;
  } else
    pos--;
  
  // now collect the actual data
  if (pos < words.size()) {
    wd = words[pos];
    int wt = lookup_word(wordmap, prim._type, prim._transp_proto, wd, errh);
    pos++;
    
    if (wt == -2)		// ambiguous or incorrect word type
      /* absorb word, but do nothing */
      prim._type = -2;

    else if (WT_TYPE(wt) == TYPE_PROTO) {
      prim._data = TYPE_PROTO;
      prim._u.u = (wt & WT_DATA);

    } else if (WT_TYPE(wt) == TYPE_PORT) {
      prim._data = TYPE_PORT;
      prim._u.u = (wt & WT_DATA);

    } else if (WT_TYPE(wt) == TYPE_TCPOPT) {
      prim._data = TYPE_TCPOPT;
      prim._u.u = (wt & WT_DATA);

    } else if (WT_TYPE(wt) == TYPE_ICMP_TYPE) {
      prim._data = TYPE_ICMP_TYPE;
      prim._u.u = (wt & WT_DATA);

    } else if (cp_integer(wd, &prim._u.i))
      prim._data = TYPE_INT;
  
    else if (cp_ip_address(wd, prim._u.c, this)) {
      if (pos < words.size() - 1 && words[pos] == "mask"
	  && cp_ip_address(words[pos+1], prim._mask.c, this)) {
	pos += 2;
	prim._data = TYPE_NET;
      } else if (prim._type == TYPE_NET && cp_ip_prefix(wd, prim._u.c, prim._mask.c, this))
	prim._data = TYPE_NET;
      else
	prim._data = TYPE_HOST;
    
    } else if (cp_ip_prefix(wd, prim._u.c, prim._mask.c, this))
      prim._data = TYPE_NET;

    else {
      if (prim._op != OP_EQ || prim._op_negated)
	errh->error("dangling operator near `%s'", wd.cc());
      pos--;
    }
  }

  if (pos == first_pos) {
    errh->error("empty term near `%s'", wd.cc());
    return pos;
  }
  
  // add if it is valid
  if (prim.check(prev_prim, errh) >= 0) {
    prim.add_exprs(this, tree);
    if (negated)
      negate_expr_subtree(tree);
    prev_prim = prim;
  }

  return pos;
}

int
IPFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _output_everything = -1;

  // requires packet headers be aligned
  _align_offset = 0;

  Vector<int> tree;
  init_expr_subtree(tree);
  
  // [QUALS] [host|net|port|proto] [data]
  // QUALS ::= src | dst | src and dst | src or dst | \empty
  //        |  ip | icmp | tcp | udp
  for (int argno = 0; argno < conf.size(); argno++) {
    Vector<String> words;
    cp_spacevec(conf[argno], words);
    separate_words(words);

    if (words.size() == 0) {
      errh->error("empty pattern %d", argno);
      continue;
    }

    PrefixErrorHandler cerrh(errh, "pattern " + String(argno) + ": ");
    
    // get slot
    int slot = noutputs();
    {
      String slotwd = cp_unquote(words[0]);
      if (slotwd == "allow") {
	slot = 0;
	if (noutputs() == 0)
	  cerrh.error("`allow' is meaningless, element has zero outputs");
      } else if (slotwd == "deny") {
	slot = noutputs();
	if (noutputs() > 1)
	  cerrh.warning("meaning of `deny' has changed (now it means `drop')");
      } else if (slotwd == "drop")
	slot = noutputs();
      else if (cp_integer(slotwd, &slot)) {
	if (slot < 0 || slot >= noutputs()) {
	  cerrh.error("slot `%d' out of range", slot);
	  slot = noutputs();
	}
      } else
	cerrh.error("unknown slot ID `%s'", slotwd.cc());
    }

    start_expr_subtree(tree);

    // check for "-"
    if (words.size() == 1 || (words.size() == 2 && words[1] == "-")
	|| (words.size() == 2 && words[1] == "any")
	|| (words.size() == 2 && words[1] == "all"))
      add_expr(tree, 0, 0, 0);

    else {
      // start with a blank primitive
      Primitive prev_prim;
      
      int pos = parse_expr(words, 1, tree, prev_prim, &cerrh);
      if (pos < words.size())
	cerrh.error("garbage after expression at `%s'", String(words[pos]).cc());
    }
    
    finish_expr_subtree(tree, true, -slot);
  }

  finish_expr_subtree(tree, false, -noutputs(), -noutputs());
  
  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }
  optimize_exprs(errh);
  //{ String sxxx = program_string(this, 0); click_chatter("%s", sxxx.cc()); }
  return 0;
}


//
// RUNNING
//

void
IPFilter::length_checked_push(Packet *p)
{
  const unsigned char *neth_data = (const unsigned char *)p->ip_header();
  const unsigned char *transph_data = (const unsigned char *)p->transport_header();
  int packet_length = p->length() + TRANSP_FAKE_OFFSET - p->transport_header_offset();
  Expr *ex = &_exprs[0];	// avoid bounds checking
  int pos = 0;
  
  do {
    int off = ex[pos].offset;
    unsigned data;
    if (off + 4 > packet_length)
      goto check_length;
    
   length_ok:
    if (off >= TRANSP_FAKE_OFFSET)
      data = *(const unsigned *)(transph_data + off - TRANSP_FAKE_OFFSET);
    else
      data = *(const unsigned *)(neth_data + off);
    if ((data & ex[pos].mask.u)	== ex[pos].value.u)
      pos = ex[pos].yes;
    else
      pos = ex[pos].no;
    continue;
    
   check_length:
    if (ex[pos].offset < packet_length) {
      unsigned available = packet_length - ex[pos].offset;
      if (!(ex[pos].mask.c[3]
	    || (ex[pos].mask.c[2] && available <= 2)
	    || (ex[pos].mask.c[1] && available == 1)))
	goto length_ok;
    }
    pos = ex[pos].no;
  } while (pos > 0);
  
  checked_output_push(-pos, p);
}

void
IPFilter::push(int, Packet *p)
{
  const unsigned char *neth_data = (const unsigned char *)p->ip_header();
  const unsigned char *transph_data = (const unsigned char *)p->transport_header();
  Expr *ex;
  int pos = 0;
  
  if (_output_everything >= 0) {
    // must use checked_output_push because the output number might be
    // out of range
    pos = -_output_everything;
    goto found;
  } else if (p->length() + TRANSP_FAKE_OFFSET - p->transport_header_offset() < _safe_length) {
    // common case never checks packet length
    length_checked_push(p);
    return;
  }
  
  ex = &_exprs[0];	// avoid bounds checking
  
  do {
    int off = ex[pos].offset;
    unsigned data;
    if (off >= TRANSP_FAKE_OFFSET)
      data = *(const unsigned *)(transph_data + off - TRANSP_FAKE_OFFSET);
    else
      data = *(const unsigned *)(neth_data + off);
    if ((data & ex[pos].mask.u)	== ex[pos].value.u)
      pos = ex[pos].yes;
    else
      pos = ex[pos].no;
  } while (pos > 0);
  
 found:
  checked_output_push(-pos, p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Classifier)
EXPORT_ELEMENT(IPFilter)
