/*
 * ipfilter.{cc,hh} -- IP-packet filter with tcpdumplike syntax
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "ipfilter.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/click_icmp.h>
#include <click/hashmap.hh>

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
  wordmap->insert("dscp",	WT(TYPE_TYPE, TYPE_DSCP));
  wordmap->insert("type",	WT(TYPE_TYPE, TYPE_ICMP_TYPE));
  wordmap->insert("frag",	WT(TYPE_TYPE, TYPE_IPFRAG));
  wordmap->insert("unfrag",	WT(TYPE_TYPE, TYPE_IPUNFRAG));
  
  wordmap->insert("icmp",	WT(TYPE_PROTO, IP_PROTO_ICMP));
  wordmap->insert("igmp",	WT(TYPE_PROTO, IP_PROTO_IGMP));
  wordmap->insert("ipip",	WT(TYPE_PROTO, IP_PROTO_IPIP));
  wordmap->insert("tcp",	WT(TYPE_PROTO, IP_PROTO_TCP));
  wordmap->insert("udp",	WT(TYPE_PROTO, IP_PROTO_UDP));
  
  wordmap->insert("echo",	WT(TYPE_PORT, 7) | WT_MORE);
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
  wordmap->insert("finger",	WT(TYPE_PORT, 79));
  wordmap->insert("www",	WT(TYPE_PORT, 80));
  wordmap->insert("pop3",	WT(TYPE_PORT, 110));
  wordmap->insert("auth",	WT(TYPE_PORT, 113));
  wordmap->insert("imap3",	WT(TYPE_PORT, 220));
  wordmap->insert("https",	WT(TYPE_PORT, 443));
  wordmap->insert("imaps",	WT(TYPE_PORT, 993));
  wordmap->insert("pop3s",	WT(TYPE_PORT, 995));

  wordmap->insert("syn",	WT(TYPE_TCPOPT, TH_SYN));
  wordmap->insert("fin",	WT(TYPE_TCPOPT, TH_FIN));
  wordmap->insert("ack",	WT(TYPE_TCPOPT, TH_ACK));
  wordmap->insert("rst",	WT(TYPE_TCPOPT, TH_RST));
  wordmap->insert("psh",	WT(TYPE_TCPOPT, TH_PUSH));
  wordmap->insert("urg",	WT(TYPE_TCPOPT, TH_URG));

  wordmap->insert("echo_reply",	WT(TYPE_ICMP_TYPE, ICMP_ECHO_REPLY));
  wordmap->insert("dst_unreachable", WT(TYPE_ICMP_TYPE, ICMP_DST_UNREACHABLE));
  wordmap->insert("source_quench", WT(TYPE_ICMP_TYPE, ICMP_SOURCE_QUENCH));
  wordmap->insert("redirect",	WT(TYPE_ICMP_TYPE, ICMP_REDIRECT));
  wordmap->insert("echo+",	WT(TYPE_ICMP_TYPE, ICMP_ECHO));
  wordmap->insert("time_exceeded", WT(TYPE_ICMP_TYPE, ICMP_TYPE_TIME_EXCEEDED));
  wordmap->insert("parameter_problem", WT(TYPE_ICMP_TYPE, ICMP_PARAMETER_PROBLEM));
  wordmap->insert("time_stamp", WT(TYPE_ICMP_TYPE, ICMP_TIME_STAMP));
  wordmap->insert("time_stamp_reply", WT(TYPE_ICMP_TYPE, ICMP_TIME_STAMP_REPLY));
  wordmap->insert("info_request", WT(TYPE_ICMP_TYPE, ICMP_INFO_REQUEST));
  wordmap->insert("info_request_reply", WT(TYPE_ICMP_TYPE, ICMP_INFO_REQUEST_REPLY));

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
IPFilter::lookup_word(HashMap<String, int> *wordmap, int type, String word, ErrorHandler *errh)
{
  String orig_word = word;
  int t = (*wordmap)[word];
  if (t == 0)
    return -1;

  // no type known? return it right away, or complain about ambiguity
  if (type == 0) {
    if (!(t & WT_MORE))
      return t;
    
    if (errh) {
      StringAccum sa;
      accum_wt_names(wordmap, word, sa);
      errh->error("`%s' is ambiguous; specify %s", word.cc(), sa.cc());
    }
    return -2;
  }

  // look for matching type
  while (t != 0) {
    if (WT_TYPE(t) == type)
      return (t & ~WT_MORE);
    word += "+";
    t = (*wordmap)[word];
  }

  // no matching type
  if (errh) {
    String tn = Primitive::unparse_type(0, type);
    StringAccum sa;
    accum_wt_names(wordmap, orig_word, sa);
    errh->error("`%s %s' is meaningless; specify %s with %s", tn.cc(), orig_word.cc(), sa.cc(), orig_word.cc());
  }
  return -2;
}

IPFilter::IPFilter()
{
  // no MOD_INC_USE_COUNT; rely on Classifier
  if (ip_filter_count == 0)
    wordmap = create_wordmap();
  ip_filter_count++;
}

IPFilter::~IPFilter()
{
  // no MOD_DEC_USE_COUNT; rely on Classifier
  ip_filter_count--;
  if (ip_filter_count == 0)
    delete wordmap;
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
  _op = 0;
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
IPFilter::Primitive::set_mask(int full_mask, int shift, ErrorHandler *errh)
{
  int data = _u.i;
  
  if (_op == OP_GT && data == 0) {
    _op = OP_EQ;
    _op_negated = !_op_negated;
  }
  
  if (_op == OP_GT || _op == OP_LT) {
    int pow2 = (_op == OP_GT ? data + 1 : data);
    if ((pow2 & (pow2 - 1)) == 0 && (pow2 - 1) <= full_mask) {
      // have a power of 2
      _u.i = 0;
      _mask = (full_mask & ~(pow2 - 1)) << shift;
      if (_op == OP_GT)
	_op_negated = !_op_negated;
      return 0;
    } else if ((_op == OP_LT && data == 0) || data >= full_mask)
      return errh->error("value %d out of range", data);
    else
      return errh->error("bad relation `%s%s %d'\n(I can only handle relations of the form `< POW', `>= POW', `<= POW-1', or\n`> POW-1' where POW is a power of 2.)", ((_op == OP_LT) ^ _op_negated ? "<" : ">"), (_op_negated ? "=" : ""), data);
  }

  if (data < 0 || data > full_mask)
    return errh->error("value %d out of range", data);

  _u.i = data << shift;
  _mask = full_mask << shift;
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
   case TYPE_NET: sa << "ip net "; break;
   case TYPE_PROTO: sa << "proto "; break;
   case TYPE_PORT: sa << "port "; break;
   case TYPE_TCPOPT: sa << "tcp opt "; break;
   case TYPE_TOS: sa << "ip tos "; break;
   case TYPE_DSCP: sa << "ip dscp "; break;
   case TYPE_ICMP_TYPE: sa << "icmp type "; break;
   case TYPE_IPFRAG: sa << "ip frag "; break;
   case TYPE_IPUNFRAG: sa << "ip unfrag "; break;
   default: sa << "<unknown type " << type << "> "; break;
  }

  sa.pop();
  return sa.take_string();
}

String
IPFilter::Primitive::unparse_type() const
{
  return unparse_type(_srcdst, _type);
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
      if (p._type == TYPE_PROTO || p._type == TYPE_PORT || p._type == TYPE_ICMP_TYPE) {
	_data = p._type;
	goto retry;
      } else
	return errh->error("bare integer `%d'; specify `proto', `port', or `icmp type'", _u.i);
      break;

     case TYPE_NONE:
      if (_transp_proto != UNKNOWN)
	_type = TYPE_PROTO;
      else
	return errh->error("syntax error: empty directive");
      break;
      
     default:
      return errh->error("unknown type `%s'", unparse_type(0, _data).cc());

    }
  }

  // clear _mask
  _mask = 0;
  
  // check that _data and _type agree
  if (_type == TYPE_HOST) {
    if (_data != TYPE_HOST)
      return errh->error("`host' directive requires IP address");
    if (_op != OP_EQ)
      return errh->error("can't use relational operators with `host'");
    
  } else if (_type == TYPE_NET) {
    if (_data != TYPE_NET)
      return errh->error("`net' directive requires IP address and mask");
    if (_op != OP_EQ)
      return errh->error("can't use relational operators with `net'");
    
  } else if (_type == TYPE_PROTO) {
    if (_data == TYPE_INT || _data == TYPE_PROTO) {
      if (_transp_proto != UNKNOWN)
	return errh->error("transport protocol specified twice");
      _data = TYPE_NONE;
    } else
      _u.i = _transp_proto;
    _transp_proto = (_op_negated || _op != OP_EQ ? (int)UNKNOWN : _u.i);
    if (_data != TYPE_NONE)
      return errh->error("`proto' directive requires IP protocol");
    if (_u.i == IP_PROTO_TCP_OR_UDP && _op != OP_EQ)
      return errh->error("can't use relational operators with `tcp or udp'");
    if (set_mask(0xFF, 0, errh) < 0)
      return -1;
    
  } else if (_type == TYPE_PORT) {
    if (_data == TYPE_INT)
      _data = TYPE_PORT;
    if (_data != TYPE_PORT)
      return errh->error("`port' directive requires port number (have %d)", _data);
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_TCP_OR_UDP;
    else if (_transp_proto != IP_PROTO_TCP && _transp_proto != IP_PROTO_UDP && _transp_proto != IP_PROTO_TCP_OR_UDP)
      return errh->error("bad protocol %d for `port' directive", _transp_proto);
    if (set_mask(0xFFFF, 0, errh) < 0)
      return -1;

  } else if (_type == TYPE_TCPOPT) {
    if (_data == TYPE_INT)
      _data = TYPE_TCPOPT;
    if (_data != TYPE_TCPOPT)
      return errh->error("`tcp opt' directive requires TCP options");
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_TCP;
    else if (_transp_proto != IP_PROTO_TCP)
      return errh->error("bad protocol %d for `tcp opt' directive", _transp_proto);
    if (_op != OP_EQ || _op_negated)
      return errh->error("can't use relational operators with `tcp opt'");
    if (_u.i < 0 || _u.i > 255)
      return errh->error("value %d out of range", _u.i);

  } else if (_type == TYPE_TOS) {
    if (_data != TYPE_INT)
      return errh->error("`ip tos' directive requires TOS value");
    if (set_mask(0xFF, 0, errh) < 0)
      return -1;

  } else if (_type == TYPE_DSCP) {
    if (_data != TYPE_INT)
      return errh->error("`ip dscp' directive requires TOS value");
    if (set_mask(0x3F, 2, errh) < 0)
      return -1;
    _type = TYPE_TOS;

  } else if (_type == TYPE_ICMP_TYPE) {
    if (_data == TYPE_INT)
      _data = TYPE_ICMP_TYPE;
    if (_data != TYPE_ICMP_TYPE)
      return errh->error("`icmp type' directive requires ICMP type");
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_ICMP;
    else if (_transp_proto != IP_PROTO_ICMP)
      return errh->error("bad protocol %d for `icmp type' directive", _transp_proto);
    if (set_mask(0xFF, 0, errh) < 0)
      return -1;
    
  } else if (_type == TYPE_IPFRAG || _type == TYPE_IPUNFRAG) {
    if (_data != TYPE_NONE)
      return errh->error("`ip frag' directive takes no data");
  }

  // fix _srcdst
  if (_type == TYPE_HOST || _type == TYPE_NET || _type == TYPE_PORT) {
    if (_srcdst == 0)
      _srcdst = SD_OR;
  } else if (old_srcdst)
    errh->warning("`src' or `dst' is meaningless here");
  
  return 0;
}

static void
add_exprs_for_proto(int proto, int mask, Classifier *c, Vector<int> &tree)
{
  if (mask == 0xFF && proto == IPFilter::IP_PROTO_TCP_OR_UDP) {
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
  bool negated = _negated;

  // handle transport protocol uniformly
  c->start_expr_subtree(tree);
  if (_type == TYPE_PROTO)
    add_exprs_for_proto(_u.i, _mask, c, tree);
  else if (_transp_proto != UNKNOWN)
    add_exprs_for_proto(_transp_proto, 0xFF, c, tree);

  // handle other types
  if (_type == TYPE_HOST || _type == TYPE_NET) {
    if (_type == TYPE_HOST) {
      e.mask.u = 0xFFFFFFFFU;
      e.value.u = _u.ip.s_addr;
    } else {
      e.mask.u = _u.ipnet.mask.s_addr;
      e.value.u = _u.ipnet.ip.s_addr & _u.ipnet.mask.s_addr;
    }
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
    
  } else if (_type == TYPE_PORT) {
    unsigned mask = (htons(_mask) << 16) | htons(_mask);
    unsigned ports = (htons(_u.i) << 16) | htons(_u.i);
    
    // enforce first fragment: fragmentation offset == 0
    c->add_expr(tree, 4, 0, htonl(0x00001FFF));

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

  } else if (_type == TYPE_TCPOPT) {
    // enforce first fragment: fragmentation offset == 0
    c->add_expr(tree, 4, 0, htonl(0x00001FFF));
    unsigned val = (negated ? 0 : _u.i);
    c->add_expr(tree, TRANSP_FAKE_OFFSET + 12, htonl(val << 16), htonl(_u.i << 16));
    negated = false;		// did our own negation

  } else if (_type == TYPE_TOS) {
    c->start_expr_subtree(tree);
    c->add_expr(tree, 0, htonl(_u.i << 16), htonl(_mask << 16));
    c->finish_expr_subtree(tree, true);
    if (_op_negated) c->negate_expr_subtree(tree);

  } else if (_type == TYPE_ICMP_TYPE) {
    // enforce first fragment: fragmentation offset == 0
    c->add_expr(tree, 4, 0, htonl(0x00001FFF));
    c->start_expr_subtree(tree);
    c->add_expr(tree, TRANSP_FAKE_OFFSET, htonl(_u.i << 24), htonl(_mask << 24));
    c->finish_expr_subtree(tree, true);
    if (_op_negated) c->negate_expr_subtree(tree);

  } else if (_type == TYPE_IPFRAG || _type == TYPE_IPUNFRAG) {
    c->start_expr_subtree(tree);
    c->add_expr(tree, 4, 0, htonl(0x00003FFF));
    c->finish_expr_subtree(tree, true);
    if (_type == TYPE_IPFRAG)
      c->negate_expr_subtree(tree);
  }

  c->finish_expr_subtree(tree, true);

  if (negated)
    c->negate_expr_subtree(tree);
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
      if (data[pos] == '(' || data[pos] == ')' || data[pos] == '!') {
	if (pos != first)
	  out.push_back(words[i].substring(first, pos - first));
	out.push_back(words[i].substring(pos, 1));
	first = pos + 1;
      } else if (data[pos] == '&' || data[pos] == '|') {
	int j = (pos < len - 1 && data[pos+1] == data[pos] ? 2 : 1);
	if (pos != first)
	  out.push_back(words[i].substring(first, pos - first));
	out.push_back(words[i].substring(pos, j));
	first = pos + j;
	pos += j - 1;
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
    int wt = lookup_word(wordmap, 0, wd, 0);

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
  String wd = (pos >= words.size() ? String() : words[pos]);
  pos++;
  prim._op = OP_EQ;
  prim._op_negated = false;
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
    int wt = lookup_word(wordmap, prim._type, wd, errh);
    pos++;
    
    if (wt == -2)		// ambiguous or incorrect word type
      /* absorb word, but do nothing */
      prim._type = -2;

    else if (WT_TYPE(wt) == TYPE_PROTO) {
      prim._data = TYPE_PROTO;
      prim._u.i = (wt & WT_DATA);

    } else if (WT_TYPE(wt) == TYPE_PORT) {
      prim._data = TYPE_PORT;
      prim._u.i = (wt & WT_DATA);

    } else if (WT_TYPE(wt) == TYPE_TCPOPT) {
      prim._data = TYPE_TCPOPT;
      prim._u.i = (wt & WT_DATA);

    } else if (WT_TYPE(wt) == TYPE_ICMP_TYPE) {
      prim._data = TYPE_ICMP_TYPE;
      prim._u.i = (wt & WT_DATA);

    } else if (cp_integer(wd, &prim._u.i))
      prim._data = TYPE_INT;
  
    else if (cp_ip_address(wd, (unsigned char *)&prim._u.ip, this)) {
      if (pos < words.size() - 1 && words[pos] == "mask"
	  && cp_ip_address(words[pos+1], (unsigned char *)&prim._u.ipnet.mask, this)) {
	pos += 2;
	prim._data = TYPE_NET;
      } else if (prim._type == TYPE_NET && cp_ip_prefix(wd, (unsigned char *)&prim._u.ipnet.ip, (unsigned char *)&prim._u.ipnet.mask, this))
	prim._data = TYPE_NET;
      else
	prim._data = TYPE_HOST;
    
    } else if (cp_ip_prefix(wd, (unsigned char *)&prim._u.ipnet.ip, (unsigned char *)&prim._u.ipnet.mask, this))
      prim._data = TYPE_NET;

    else
      pos--;
  }

  if (pos == first_pos) {
    errh->error("empty term near `%s'", wd.cc());
    return pos;
  }
  
  // add if it is valid
  prim._negated = negated;
  if (prim.check(prev_prim, errh) >= 0) {
    prim.add_exprs(this, tree);
    prev_prim = prim;
  }

  return pos;
}

int
IPFilter::configure(const Vector<String> &conf, ErrorHandler *errh)
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
      } else if (slotwd == "deny")
	slot = 1;
      else if (slotwd == "drop")
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
	cerrh.error("garbage after expression near `%s'", String(words[pos]).cc());
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
  
  checked_push_output(-pos, p);
}

void
IPFilter::push(int, Packet *p)
{
  const unsigned char *neth_data = (const unsigned char *)p->ip_header();
  const unsigned char *transph_data = (const unsigned char *)p->transport_header();
  Expr *ex = &_exprs[0];	// avoid bounds checking
  int pos = 0;
  
  if (_output_everything >= 0) {
    // must use checked_push_output because the output number might be
    // out of range
    pos = -_output_everything;
    goto found;
  } else if (p->length() + TRANSP_FAKE_OFFSET - p->transport_header_offset() < _safe_length) {
    // common case never checks packet length
    length_checked_push(p);
    return;
  }
  
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
  checked_push_output(-pos, p);
}

ELEMENT_REQUIRES(Classifier)
EXPORT_ELEMENT(IPFilter)
