/*
 * ipclassifier.{cc,hh} -- IP-packet classifier with tcpdumplike syntax
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipclassifier.hh"
#include "glue.hh"
#include "error.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "click_ip.h"
#include "click_tcp.h"
#include "hashmap.hh"

IPClassifier::IPClassifier()
{
}

IPClassifier::~IPClassifier()
{
}

IPClassifier *
IPClassifier::clone() const
{
  return new IPClassifier;
}

//
// CONFIGURATION
//

void
IPClassifier::Primitive::clear()
{
  _type = _srcdst = 0;
  _net_proto = _transp_proto = UNKNOWN;
  _data = 0;
  _op = 0;
}

void
IPClassifier::Primitive::set_type(int x, int slot, ErrorHandler *errh)
{
  if (_type)
    errh->error("pattern %d: type specified twice", slot);
  _type = x;
}

void
IPClassifier::Primitive::set_srcdst(int x, int slot, ErrorHandler *errh)
{
  if (_srcdst)
    errh->error("pattern %d: `src' or `dst' specified twice", slot);
  _srcdst = x;
}

void
IPClassifier::Primitive::set_net_proto(int x, int slot, ErrorHandler *errh)
{
  if (_net_proto != UNKNOWN && _net_proto != x)
    errh->error("pattern %d: network protocol specified twice", slot);
  _net_proto = x;
}

void
IPClassifier::Primitive::set_transp_proto(int x, int slot, ErrorHandler *errh)
{
  if (_transp_proto != UNKNOWN)
    errh->error("pattern %d: transport protocol specified twice", slot);
  _transp_proto = x;
}

int
IPClassifier::Primitive::set_mask(int *data_store, int full_mask, int shift, int slot, ErrorHandler *errh)
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
IPClassifier::Primitive::check(int slot, const Primitive &p, ErrorHandler *errh)
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
IPClassifier::Primitive::add_exprs(Classifier *c, Vector<int> &tree) const
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

int
IPClassifier::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  set_noutputs(conf.size());

  _output_everything = -1;

  // requires packet headers be aligned
  _align_offset = 0;

  // create IP protocol hashmap
  HashMap<String, int> type_map(-1);
  type_map.insert("host", TYPE_HOST);
  type_map.insert("net", TYPE_NET);
  type_map.insert("port", TYPE_PORT);
  type_map.insert("proto", TYPE_PROTO);
  type_map.insert("opt", TYPE_TCPOPT);
  type_map.insert("tos", TYPE_TOS);
  type_map.insert("dscp", TYPE_DSCP);
  type_map.insert("type", TYPE_ICMP_TYPE);
  type_map.insert("frag", TYPE_IPFRAG);
  type_map.insert("unfrag", TYPE_IPUNFRAG);
  
  HashMap<String, int> ip_proto_map(-1);
  ip_proto_map.insert("icmp", IP_PROTO_ICMP);
  ip_proto_map.insert("igmp", IP_PROTO_IGMP);
  ip_proto_map.insert("ipip", IP_PROTO_IPIP);
  ip_proto_map.insert("tcp", IP_PROTO_TCP);
  ip_proto_map.insert("udp", IP_PROTO_UDP);
  
  HashMap<String, int> ip_port_map(-1);
  ip_port_map.insert("echo", 7);
  ip_port_map.insert("discard", 9);
  ip_port_map.insert("daytime", 13);
  ip_port_map.insert("chargen", 19);
  ip_port_map.insert("ftp-data", 20);
  ip_port_map.insert("ftp", 21);
  ip_port_map.insert("ssh", 22);
  ip_port_map.insert("telnet", 23);
  ip_port_map.insert("smtp", 25);
  ip_port_map.insert("domain", 53);
  ip_port_map.insert("dns", 53);
  ip_port_map.insert("finger", 79);
  ip_port_map.insert("www", 80);
  ip_port_map.insert("auth", 113);
  ip_port_map.insert("https", 443);

  HashMap<String, int> tcp_opt_map(-1);
  tcp_opt_map.insert("syn", TH_SYN);
  tcp_opt_map.insert("fin", TH_FIN);
  tcp_opt_map.insert("ack", TH_ACK);
  tcp_opt_map.insert("rst", TH_RST);
  tcp_opt_map.insert("psh", TH_PUSH);
  tcp_opt_map.insert("urg", TH_URG);

  Vector<int> tree;
  init_expr_subtree(tree);
  
  // [QUALS] [host|net|port|proto] [data]
  // QUALS ::= src | dst | src and dst | src or dst | \empty
  //        |  ip | icmp | tcp | udp
  for (int slot = 0; slot < conf.size(); slot++) {
    Vector<String> words;
    cp_spacevec(conf[slot], words);
    separate_words(words);

    // parse parens
    Vector<int> parens;
    parens.push_back(0);
    Vector<int> negated;
    negated.push_back(0);
    start_expr_subtree(tree);

    // check for "-"
    if (words.size() == 1 && words[0] == "-") {
      add_expr(tree, 0, 0, 0);
      finish_expr_subtree(tree, parens.back() != '|', -slot);
      continue;
    }
    
    // start with a blank primitive
    Primitive prev_prim;

    // loop over primitives
    for (int w = 0; w < words.size(); w++) {
      Primitive prim;

      // check for `not' and parentheses
      bool negate = false, just_negated = false;
      while (w < words.size() && (words[w] == "not" || words[w] == "!")) {
	negate = !negate;
	just_negated = true;
	w++;
      }
      if (w < words.size() && words[w] == "(") {
	start_expr_subtree(tree);
	parens.push_back(0);
	negated.push_back(negate);
	continue;
      } else if (w < words.size() && words[w] == ")") {
	if (just_negated)
	  errh->error("pattern %d: dangling `not'", slot);
	if (parens.size() == 1)
	  errh->error("pattern %d: too many `)'s", slot);
	else {
	  finish_expr_subtree(tree, parens.back() != '|');
	  if (negated.back()) negate_expr_subtree(tree);
	  parens.pop_back();
	  negated.pop_back();
	}
	continue;
      }

      // collect qualifiers
      for (; w < words.size(); w++) {
	String wd = words[w];
	if (type_map[wd] >= 0)
	  prim.set_type(type_map[wd], slot, errh);

	else if (wd == "src") {
	  if (w < words.size() - 2 && (words[w+2] == "dst" || words[w+2] == "dest")) {
	    if (words[w+1] == "and" || words[w+1] == "&&")
	      prim.set_srcdst(SD_AND, slot, errh);
	    else if (words[w+1] == "or" || words[w+1] == "||")
	      prim.set_srcdst(SD_OR, slot, errh);
	    else
	      prim.set_srcdst(SD_SRC, slot, errh);
	    w += 2;
	  } else
	    prim.set_srcdst(SD_SRC, slot, errh);
	} else if (wd == "dst" || wd == "dest")
	  prim.set_srcdst(SD_DST, slot, errh);

	else if (wd == "ip")
	  prim.set_net_proto(PROTO_IP, slot, errh);

	else if (ip_proto_map[wd] >= 0) {
	  prim.set_net_proto(PROTO_IP, slot, errh);
	  prim.set_transp_proto(ip_proto_map[wd], slot, errh);

	} else if (wd == "not" || wd == "!") {
	  negate = !negate;
	
	} else
	  break;
      }
      // end collect qualifiers

      // optional relational operation
      String wd = (w >= words.size() ? String() : words[w]);
      w++;
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
	w--;
      
      // now collect the actual data
      wd = (w >= words.size() ? String() : words[w]);
      if (w >= words.size())
	/* no extra data */;
      
      else if (wd == "and" || wd == "or" || wd == "&&" || wd == "||"
	       || wd == "(" || wd == ")") {
	/* no extra data */
	w--;
	
      } else if (cp_integer(wd, &prim._u.i))
	prim._data = DATA_INT;
      
      else if ((prim._u.i = ip_proto_map[wd]) >= 0)
	prim._data = DATA_PROTO;
      
      else if ((prim._u.i = ip_port_map[wd]) >= 0)
	prim._data = DATA_PORT;
      
      else if ((prim._u.i = tcp_opt_map[wd]) >= 0)
	prim._data = DATA_TCPOPT;
      
      else if (cp_ip_address(wd, (unsigned char *)&prim._u.ip, this)) {
	if (w < words.size() - 2 && words[w+1] == "mask"
	    && cp_ip_address(words[w+2], (unsigned char *)&prim._u.ipnet.mask, this)) {
	  w += 2;
	  prim._data = DATA_IPNET;
	} else if (prim._type == TYPE_NET && cp_ip_prefix(wd, (unsigned char *)&prim._u.ipnet.ip, (unsigned char *)&prim._u.ipnet.mask, this))
	  prim._data = DATA_IPNET;
	else
	  prim._data = DATA_IP;
	
      } else if (cp_ip_prefix(wd, (unsigned char *)&prim._u.ipnet.ip, (unsigned char *)&prim._u.ipnet.mask, this))
	prim._data = DATA_IPNET;
      
      else
	errh->error("pattern %d: syntax error near `%s'", slot, wd.cc());

      // add if it is valid
      prim._negated = negate;
      if (prim.check(slot, prev_prim, errh) >= 0) {
	prim.add_exprs(this, tree);
	prev_prim = prim;
      }
      
      // check for combining expression
      while (w < words.size() - 1) {
	String wd = words[w+1];
	int next_combiner = 0;
	if (words[w+1] == "and" || words[w+1] == "&&")
	  next_combiner = '&';
	else if (words[w+1] == "or" || words[w+1] == "||")
	  next_combiner = '|';
	else
	  break;
	if (parens.back() && next_combiner != parens.back())
	  errh->error("pattern %d: must use parens to combine `and' and `or'", slot);
	parens.back() = next_combiner;
	w++;
      }
    }

    if (parens.size() > 1)
      errh->error("pattern %d: too many `('s", slot);
    while (parens.size() > 1) {
      finish_expr_subtree(tree, parens.back() != '|');
      if (negated.back()) negate_expr_subtree(tree);
      parens.pop_back();
      negated.pop_back();
    }
    finish_expr_subtree(tree, parens.back() != '|', -slot);
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
IPClassifier::length_checked_push(Packet *p)
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
IPClassifier::push(int, Packet *p)
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
EXPORT_ELEMENT(IPClassifier)
