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
#include <click/hashmap.hh>

static HashMap<String, int> *wordmap;
static int ip_filter_count;

void
IPFilter::initialize_wordmap()
{
  wordmap = new HashMap<String, int>(0);

  wordmap->insert("host",	WT_TYPE | TYPE_HOST);
  wordmap->insert("net",	WT_TYPE | TYPE_NET);
  wordmap->insert("port",	WT_TYPE | TYPE_PORT);
  wordmap->insert("proto",	WT_TYPE | TYPE_PROTO);
  wordmap->insert("opt",	WT_TYPE | TYPE_TCPOPT);
  wordmap->insert("tos",	WT_TYPE | TYPE_TOS);
  wordmap->insert("dscp",	WT_TYPE | TYPE_DSCP);
  wordmap->insert("type",	WT_TYPE | TYPE_ICMP_TYPE);
  wordmap->insert("frag",	WT_TYPE | TYPE_IPFRAG);
  wordmap->insert("unfrag",	WT_TYPE | TYPE_IPUNFRAG);
  
  wordmap->insert("icmp",	WT_PROTO | IP_PROTO_ICMP);
  wordmap->insert("igmp",	WT_PROTO | IP_PROTO_IGMP);
  wordmap->insert("ipip",	WT_PROTO | IP_PROTO_IPIP);
  wordmap->insert("tcp",	WT_PROTO | IP_PROTO_TCP);
  wordmap->insert("udp",	WT_PROTO | IP_PROTO_UDP);
  
  wordmap->insert("echo",	WT_PORT | 7);
  wordmap->insert("discard",	WT_PORT | 9);
  wordmap->insert("daytime",	WT_PORT | 13);
  wordmap->insert("chargen",	WT_PORT | 19);
  wordmap->insert("ftp-data",	WT_PORT | 20);
  wordmap->insert("ftp",	WT_PORT | 21);
  wordmap->insert("ssh",	WT_PORT | 22);
  wordmap->insert("telnet",	WT_PORT | 23);
  wordmap->insert("smtp",	WT_PORT | 25);
  wordmap->insert("domain",	WT_PORT | 53);
  wordmap->insert("dns",	WT_PORT | 53);
  wordmap->insert("finger",	WT_PORT | 79);
  wordmap->insert("www",	WT_PORT | 80);
  wordmap->insert("auth",	WT_PORT | 113);
  wordmap->insert("https",	WT_PORT | 443);

  wordmap->insert("syn",	WT_TCPOPT | TH_SYN);
  wordmap->insert("fin",	WT_TCPOPT | TH_FIN);
  wordmap->insert("ack",	WT_TCPOPT | TH_ACK);
  wordmap->insert("rst",	WT_TCPOPT | TH_RST);
  wordmap->insert("psh",	WT_TCPOPT | TH_PUSH);
  wordmap->insert("urg",	WT_TCPOPT | TH_URG);
}

IPFilter::IPFilter()
{
  // no MOD_INC_USE_COUNT; rely on Classifier
  if (ip_filter_count == 0)
    initialize_wordmap();
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
  _net_proto = _transp_proto = UNKNOWN;
  _data = 0;
  _op = 0;
}

void
IPFilter::Primitive::set_type(int x, int slot, ErrorHandler *errh)
{
  if (_type)
    errh->error("pattern %d: type specified twice", slot);
  _type = x;
}

void
IPFilter::Primitive::set_srcdst(int x, int slot, ErrorHandler *errh)
{
  if (_srcdst)
    errh->error("pattern %d: `src' or `dst' specified twice", slot);
  _srcdst = x;
}

void
IPFilter::Primitive::set_net_proto(int x, int slot, ErrorHandler *errh)
{
  if (_net_proto != UNKNOWN && _net_proto != x)
    errh->error("pattern %d: network protocol specified twice", slot);
  _net_proto = x;
}

void
IPFilter::Primitive::set_transp_proto(int x, int slot, ErrorHandler *errh)
{
  if (_transp_proto != UNKNOWN)
    errh->error("pattern %d: transport protocol specified twice", slot);
  _transp_proto = x;
}

int
IPFilter::Primitive::set_mask(int *data_store, int full_mask, int shift, int slot, ErrorHandler *errh)
{
  int data = *data_store;
  
  if (_op == OP_GT && data == 0) {
    _op = OP_EQ;
    _op_negated = !_op_negated;
  }
  
  if (_op == OP_GT || _op == OP_LT) {
    int pow2 = (_op == OP_GT ? data + 1 : data);
    if ((pow2 & (pow2 - 1)) == 0 && (pow2 - 1) <= full_mask) {
      // have a power of 2
      *data_store = 0;
      _mask = (full_mask & ~(pow2 - 1)) << shift;
      if (_op == OP_GT)
	_op_negated = !_op_negated;
      return 0;
    } else if ((_op == OP_LT && data == 0) || (_op == OP_GT && data >= full_mask))
      return errh->error("pattern %d: value %d out of range", slot, data);
    else
      return errh->error("pattern %d: bad relation `%s%s %d'\n\
(I can only handle relations of the form `< POW', `>= POW', `<= POW-1', or\n\
`> POW-1' where POW is a power of 2.)", slot, ((_op == OP_LT) ^ _op_negated ? "<" : ">"), (_op_negated ? "=" : ""), data);
  }

  if (data < 0 || data > full_mask)
    return errh->error("pattern %d: value %d out of range", slot, data);

  *data_store = data << shift;
  _mask = full_mask << shift;
  return 0;
}

int
IPFilter::Primitive::check(int slot, const Primitive &p, ErrorHandler *errh)
{
  int old_srcdst = _srcdst;
  
  // set _type if it was not specified
  if (!_type) {
    if (_data == DATA_IP) {
      _type = TYPE_HOST;
      if (!_srcdst) _srcdst = p._srcdst;
    } else if (_data == DATA_IPNET) {
      _type = TYPE_NET;
      if (!_srcdst) _srcdst = p._srcdst;
    } else if (_data == DATA_PROTO || (_data == DATA_INT && p._type == TYPE_PROTO)) {
      _type = TYPE_PROTO;
      if (_net_proto == UNKNOWN) _net_proto = p._net_proto;
    } else if (_data == DATA_PORT || (_data == DATA_INT && p._type == TYPE_PORT)) {
      _type = TYPE_PORT;
      if (!_srcdst) _srcdst = p._srcdst;
      if (_net_proto == UNKNOWN) _net_proto = p._net_proto;
      if (_transp_proto == UNKNOWN) _transp_proto = p._transp_proto;
    } else if (_data == DATA_TCPOPT) {
      _type = TYPE_TCPOPT;
      if (!_srcdst) _srcdst = p._srcdst;
    } else if (_data == DATA_ICMP_TYPE || (_data == DATA_INT && p._type == TYPE_ICMP_TYPE)) {
      _type = TYPE_ICMP_TYPE;
      if (!_srcdst) _srcdst = p._srcdst;
    } else if (!_data && _transp_proto != UNKNOWN)
      _type = TYPE_PROTO;
    else
      return errh->error("pattern %d: syntax error, no primitive type", slot);
  }

  // clear _mask
  _mask = 0;
  
  // check that _data and _type agree
  if (_type == TYPE_HOST) {
    if (_data != DATA_IP)
      return errh->error("pattern %d: `host' directive requires IP address", slot);
    if (_op != OP_EQ)
      return errh->error("pattern %d: can't use relational operators with `host'", slot);
    
  } else if (_type == TYPE_NET) {
    if (_data != DATA_IPNET)
      return errh->error("pattern %d: `net' directive requires IP address and mask", slot);
    if (_op != OP_EQ)
      return errh->error("pattern %d: can't use relational operators with `net'", slot);
    
  } else if (_type == TYPE_PROTO) {
    if (_data == DATA_INT || _data == DATA_PROTO) {
      if (_transp_proto != UNKNOWN)
	return errh->error("pattern %d: transport protocol specified twice", slot);
      _transp_proto = _u.i;
      _data = DATA_NONE;
    }
    if (_data != DATA_NONE)
      return errh->error("pattern %d: `proto' directive requires IP protocol", slot);
    if (_transp_proto != IP_PROTO_TCP_OR_UDP && _op != OP_EQ)
      return errh->error("pattern %d: can't use relational operators with `tcp or udp'", slot);
    if (set_mask(&_transp_proto, 0xFF, 0, slot, errh) < 0)
      return -1;
    
  } else if (_type == TYPE_PORT) {
    if (_data == DATA_INT)
      _data = DATA_PORT;
    if (_data != DATA_PORT)
      return errh->error("pattern %d: `port' directive requires port number (have %d)", slot, _data);
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_TCP_OR_UDP;
    else if (_transp_proto != IP_PROTO_TCP && _transp_proto != IP_PROTO_UDP && _transp_proto != IP_PROTO_TCP_OR_UDP)
      return errh->error("pattern %d: bad protocol %d for `port' directive", slot, _transp_proto);
    if (set_mask(&_u.i, 0xFFFF, 0, slot, errh) < 0)
      return -1;

  } else if (_type == TYPE_TCPOPT) {
    if (_data == DATA_INT)
      _data = DATA_TCPOPT;
    if (_data != DATA_TCPOPT)
      return errh->error("pattern %d: `tcp opt' directive requires TCP options", slot);
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_TCP;
    else if (_transp_proto != IP_PROTO_TCP)
      return errh->error("pattern %d: bad protocol %d for `tcp opt' directive", slot, _transp_proto);
    if (_op != OP_EQ || _op_negated)
      return errh->error("pattern %d: can't use relational operators with `tcp opt'", slot);
    if (_u.i < 0 || _u.i > 255)
      return errh->error("pattern %d: value %d out of range", slot, _u.i);

  } else if (_type == TYPE_TOS) {
    if (_data != DATA_INT)
      return errh->error("pattern %d: `ip tos' directive requires TOS value", slot);
    if (set_mask(&_u.i, 0xFF, 0, slot, errh) < 0)
      return -1;

  } else if (_type == TYPE_DSCP) {
    if (_data != DATA_INT)
      return errh->error("pattern %d: `ip dscp' directive requires TOS value", slot);
    if (set_mask(&_u.i, 0x3F, 2, slot, errh) < 0)
      return -1;
    _type = TYPE_TOS;

  } else if (_type == TYPE_ICMP_TYPE) {
    if (_data == DATA_INT)
      _data = DATA_ICMP_TYPE;
    if (_data != DATA_ICMP_TYPE)
      return errh->error("pattern %d: `icmp type' directive requires ICMP type", slot);
    if (_transp_proto == UNKNOWN)
      _transp_proto = IP_PROTO_ICMP;
    else if (_transp_proto != IP_PROTO_ICMP)
      return errh->error("pattern %d: bad protocol %d for `icmp type' directive", slot, _transp_proto);
    if (set_mask(&_u.i, 0xFF, 0, slot, errh) < 0)
      return -1;
    
  } else if (_type == TYPE_IPFRAG || _type == TYPE_IPUNFRAG) {
    if (_data != DATA_NONE)
      return errh->error("pattern %d: `ip frag' directive takes no data", slot);
  }

  // fix _srcdst
  if (_type == TYPE_HOST || _type == TYPE_NET || _type == TYPE_PORT) {
    if (_srcdst == 0)
      _srcdst = SD_OR;
  } else if (old_srcdst)
    errh->warning("pattern %d: `src' or `dst' is meaningless here", slot);
  
  return 0;
}

void
IPFilter::Primitive::add_exprs(Classifier *c, Vector<int> &tree) const
{
  Expr e;
  bool negated = _negated;

  // handle transport protocol uniformly
  c->start_expr_subtree(tree);
  if (_transp_proto != UNKNOWN) {
    if (_transp_proto == IP_PROTO_TCP_OR_UDP) {
      c->start_expr_subtree(tree);
      c->add_expr(tree, 8, htonl(IP_PROTO_TCP << 16), htonl(0x00FF0000));
      c->add_expr(tree, 8, htonl(IP_PROTO_UDP << 16), htonl(0x00FF0000));
      c->finish_expr_subtree(tree, false);
    } else {
      unsigned mask = (_type == TYPE_PROTO ? _mask : 0xFF);
      c->add_expr(tree, 8, htonl(_transp_proto << 16), htonl(mask << 16));
    }
  }
  
  if (_type == TYPE_HOST || _type == TYPE_NET) {
    e.mask.u = (_type == TYPE_NET ? _u.ipnet.mask.s_addr : 0xFFFFFFFFU);
    e.value.u = _u.ip.s_addr;
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
		     ErrorHandler *errh, int argno)
{
  start_expr_subtree(tree);

  while (1) {
    pos = parse_term(words, pos, tree, prev_prim, errh, argno);
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
		     ErrorHandler *errh, int argno)
{
  start_expr_subtree(tree);

  bool blank_ok = false;
  while (1) {
    int next = parse_factor(words, pos, tree, prev_prim, false, errh, argno);
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
    errh->error("pattern %d: missing term", argno);
  finish_expr_subtree(tree, true);
  return pos;
}

int
IPFilter::parse_factor(const Vector<String> &words, int pos,
		       Vector<int> &tree, Primitive &prev_prim,
		       bool negated, ErrorHandler *errh, int argno)
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
    int next = parse_factor(words, pos + 1, tree, prev_prim, !negated, errh, argno);
    if (next == pos + 1)
      errh->error("pattern %d: missing factor after `%s'", argno, String(words[pos]).cc());
    return next;
  }
  // ( expr )
  if (words[pos] == "(") {
    int next = parse_expr(words, pos + 1, tree, prev_prim, errh, argno);
    if (next == pos + 1)
      errh->error("pattern %d: missing expression after `('", argno);
    if (next >= 0) {
      if (next >= words.size() || words[next] != ")")
	errh->error("pattern %d: missing `)'", argno);
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
    int wt = (*wordmap)[wd];

    if ((wt & WT_TYPE_MASK) == WT_TYPE)
      prim.set_type(wt & WT_DATA, argno, errh);

    else if ((wt & WT_TYPE_MASK) == WT_PROTO) {
      prim.set_net_proto(PROTO_IP, argno, errh);
      prim.set_transp_proto(wt & WT_DATA, argno, errh);

    } else if (wt & WT_TYPE_MASK)
      break;

    else if (wd == "src") {
      if (pos < words.size() - 2 && (words[pos+2] == "dst" || words[pos+2] == "dest")) {
	if (words[pos+1] == "and" || words[pos+1] == "&&") {
	  prim.set_srcdst(SD_AND, argno, errh);
	  pos += 2;
	} else if (words[pos+1] == "or" || words[pos+1] == "||") {
	  prim.set_srcdst(SD_OR, argno, errh);
	  pos += 2;
	} else
	  prim.set_srcdst(SD_SRC, argno, errh);
      } else
	prim.set_srcdst(SD_SRC, argno, errh);
    } else if (wd == "dst" || wd == "dest")
      prim.set_srcdst(SD_DST, argno, errh);
    
    else if (wd == "ip")
      prim.set_net_proto(PROTO_IP, argno, errh);
    
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
    int wt = (*wordmap)[wd];
    pos++;

    if ((wt & WT_TYPE_MASK) == WT_PROTO) {
      prim._data = DATA_PROTO;
      prim._u.i = (wt & WT_DATA);

    } else if ((wt & WT_TYPE_MASK) == WT_PORT) {
      prim._data = DATA_PORT;
      prim._u.i = (wt & WT_DATA);

    } else if ((wt & WT_TYPE_MASK) == WT_TCPOPT) {
      prim._data = DATA_TCPOPT;
      prim._u.i = (wt & WT_DATA);

    } else if (cp_integer(wd, &prim._u.i))
      prim._data = DATA_INT;
  
    else if (cp_ip_address(wd, (unsigned char *)&prim._u.ip, this)) {
      if (pos < words.size() - 1 && words[pos] == "mask"
	  && cp_ip_address(words[pos+1], (unsigned char *)&prim._u.ipnet.mask, this)) {
	pos += 2;
	prim._data = DATA_IPNET;
      } else if (prim._type == TYPE_NET && cp_ip_prefix(wd, (unsigned char *)&prim._u.ipnet.ip, (unsigned char *)&prim._u.ipnet.mask, this))
	prim._data = DATA_IPNET;
      else
	prim._data = DATA_IP;
    
    } else if (cp_ip_prefix(wd, (unsigned char *)&prim._u.ipnet.ip, (unsigned char *)&prim._u.ipnet.mask, this))
      prim._data = DATA_IPNET;

    else
      pos--;
  }

  if (pos == first_pos)
    return errh->error("pattern %d: empty term near `%s'", argno, wd.cc());
  
  // add if it is valid
  prim._negated = negated;
  if (prim.check(argno, prev_prim, errh) >= 0) {
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
    cp_spacevec(conf[argno].lower(), words);
    separate_words(words);

    if (words.size() == 0) {
      errh->error("empty pattern %d", argno);
      continue;
    }

    // get slot
    int slot = noutputs();
    {
      String slotwd = cp_unquote(words[0]);
      if (slotwd == "allow") {
	slot = 0;
	if (noutputs() == 0)
	  errh->error("`allow' is meaningless, element has zero outputs");
      } else if (slotwd == "deny")
	slot = 1;
      else if (slotwd == "drop")
	slot = noutputs();
      else if (cp_integer(slotwd, &slot)) {
	if (slot < 0 || slot >= noutputs()) {
	  errh->error("slot `%d' out of range in pattern %d", slot, argno);
	  slot = noutputs();
	}
      } else
	errh->error("unknown slot ID `%s' in pattern %d", slotwd.cc(), argno);
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
      
      int pos = parse_expr(words, 1, tree, prev_prim, errh, argno);
      if (pos < words.size())
	errh->error("pattern %d: garbage after expression near `%s'", argno, String(words[pos]).cc());
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
