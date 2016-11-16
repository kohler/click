/*
 * ipfilter.{cc,hh} -- IP-packet filter with tcpdumplike syntax
 * Eddie Kohler
 *
 * Copyright (c) 2000-2007 Mazu Networks, Inc.
 * Copyright (c) 2010 Meraki, Inc.
 * Copyright (c) 2004-2011 Regents of the University of California
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
#include <click/args.hh>
#include <click/straccum.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/icmp.h>
#include <click/integers.hh>
#include <click/etheraddress.hh>
#include <click/nameinfo.hh>
CLICK_DECLS

static const StaticNameDB::Entry type_entries[] = {
    { "ce", IPFilter::TYPE_IPCE },
    { "dest", IPFilter::TYPE_SYNTAX },
    { "dscp", IPFilter::FIELD_DSCP },
    { "dst", IPFilter::TYPE_SYNTAX },
    { "ect", IPFilter::TYPE_IPECT },
    { "ether", IPFilter::TYPE_SYNTAX },
    { "frag", IPFilter::TYPE_IPFRAG },
    { "hl", IPFilter::FIELD_HL },
    { "host", IPFilter::TYPE_HOST },
    { "id", IPFilter::FIELD_ID },
    { "ip", IPFilter::TYPE_SYNTAX },
    { "len", IPFilter::FIELD_IPLEN },
    { "net", IPFilter::TYPE_NET },
    { "not", IPFilter::TYPE_SYNTAX },
    { "opt", IPFilter::TYPE_TCPOPT },
    { "port", IPFilter::TYPE_PORT },
    { "proto", IPFilter::TYPE_PROTO },
    { "src", IPFilter::TYPE_SYNTAX },
    { "tos", IPFilter::FIELD_TOS },
    { "ttl", IPFilter::FIELD_TTL },
    { "type", IPFilter::FIELD_ICMP_TYPE },
    { "unfrag", IPFilter::TYPE_IPUNFRAG },
    { "vers", IPFilter::FIELD_VERSION },
    { "win", IPFilter::FIELD_TCP_WIN }
};

static const StaticNameDB::Entry tcp_opt_entries[] = {
    { "ack", TH_ACK },
    { "fin", TH_FIN },
    { "psh", TH_PUSH },
    { "rst", TH_RST },
    { "syn", TH_SYN },
    { "urg", TH_URG }
};

static const uint32_t db2type[] = {
    IPFilter::TYPE_PROTO, IPFilter::TYPE_PORT, IPFilter::TYPE_PORT,
    IPFilter::TYPE_TCPOPT, IPFilter::FIELD_ICMP_TYPE
};

static String
unparse_word(int type, int proto, const String &word)
{
    String tn = IPFilter::Primitive::unparse_type(0, type);
    String tr = IPFilter::Primitive::unparse_transp_proto(proto);
    if (tn)
	tn += " ";
    if (tr || (word && tn))
	tr += " ";
    return tn + tr + word;
}

int
IPFilter::lookup(String word, int type, int proto, uint32_t &data,
		 const Element *context, ErrorHandler *errh)
{
    // type queries always win if they occur
    if (type == 0 || type == TYPE_TYPE)
	if (NameInfo::query(NameInfo::T_IPFILTER_TYPE, context, word, &data, sizeof(uint32_t)))
	    return (data == TYPE_SYNTAX ? -1 : TYPE_TYPE);

    // query each relevant database
    int got[5];
    int32_t val[5];
    got[0] = NameInfo::query(NameInfo::T_IP_PROTO, context, word, &val[0], sizeof(uint32_t));
    got[1] = NameInfo::query(NameInfo::T_TCP_PORT, context, word, &val[1], sizeof(uint32_t));
    got[2] = NameInfo::query(NameInfo::T_UDP_PORT, context, word, &val[2], sizeof(uint32_t));
    got[3] = NameInfo::query(NameInfo::T_TCP_OPT, context, word, &val[3], sizeof(uint32_t));
    got[4] = NameInfo::query(NameInfo::T_ICMP_TYPE, context, word, &val[4], sizeof(uint32_t));

    // exit if no match
    if (!got[0] && !got[1] && !got[2] && !got[3] && !got[4])
	return -1;

    // filter
    int tgot[5];
    tgot[0] = got[0] && (type == 0 || type == TYPE_PROTO);
    tgot[1] = got[1] && (type == 0 || type == TYPE_PORT)
	&& (proto == UNKNOWN || proto == IP_PROTO_TCP || proto == IP_PROTO_TCP_OR_UDP);
    tgot[2] = got[2] && (type == 0 || type == TYPE_PORT)
	&& (proto == UNKNOWN || proto == IP_PROTO_UDP || proto == IP_PROTO_TCP_OR_UDP);
    tgot[3] = got[3] && (type == 0 || type == TYPE_TCPOPT)
	&& (proto == UNKNOWN || proto == IP_PROTO_TCP || proto == IP_PROTO_TCP_OR_UDP);
    tgot[4] = got[4] && (type == 0 || type == FIELD_ICMP_TYPE)
	&& (proto == UNKNOWN || proto == IP_PROTO_ICMP);

    // remove one of TCP and UDP port if they give the same value
    if (tgot[1] && tgot[2] && val[1] == val[2])
	tgot[2] = false;

    // return
    int ngot = tgot[0] + tgot[1] + tgot[2] + tgot[3] + tgot[4];
    if (ngot == 1) {
	for (int i = 0; i < 5; i++)
	    if (tgot[i]) {
		data = val[i];
		return db2type[i];
	    }
    }
    StringAccum sa;
    for (int i = 0; i < 5; i++)
	if (got[i]) {
	    if (sa)
		sa << ", ";
	    sa << '\'' << unparse_word(db2type[i], proto, word) << '\'';
	}
    if (errh)
	errh->error("%<%s%> is %s; try %s", unparse_word(type, proto, word).c_str(), (ngot > 1 ? "ambiguous" : "meaningless"), sa.c_str());
    return -2;
}

static NameDB *dbs[2];

void
IPFilter::static_initialize()
{
    dbs[0] = new StaticNameDB(NameInfo::T_IPFILTER_TYPE, String(), type_entries, sizeof(type_entries) / sizeof(type_entries[0]));
    dbs[1] = new StaticNameDB(NameInfo::T_TCP_OPT, String(), tcp_opt_entries, sizeof(tcp_opt_entries) / sizeof(tcp_opt_entries[0]));
    NameInfo::installdb(dbs[0], 0);
    NameInfo::installdb(dbs[1], 0);
}

void
IPFilter::static_cleanup()
{
    delete dbs[0];
    delete dbs[1];
}


IPFilter::IPFilter()
{
}

IPFilter::~IPFilter()
{
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
    errh->error("%<src%> or %<dst%> specified twice");
  _srcdst = x;
}

void
IPFilter::Primitive::set_transp_proto(int x, ErrorHandler *errh)
{
  if (_transp_proto != UNKNOWN && _transp_proto != x)
    errh->error("transport protocol specified twice");
  _transp_proto = x;
}

int
IPFilter::Primitive::set_mask(uint32_t full_mask, int shift, uint32_t provided_mask, ErrorHandler *errh)
{
    uint32_t data = _u.u;
    uint32_t this_mask = (provided_mask ? provided_mask : full_mask);
    if ((this_mask & full_mask) != this_mask)
	return errh->error("%<%s%>: mask out of range, bound 0x%X", unparse_type().c_str(), full_mask);

    if (_op == OP_GT || _op == OP_LT) {
	// Check for comparisons that are always true or false.
	if ((_op == OP_LT && (data == 0 || data > this_mask))
	    || (_op == OP_GT && data >= this_mask)) {
	    bool will_be = (_op == OP_LT && data > this_mask ? !_op_negated : _op_negated);
	    errh->warning("relation %<%s %u%> is always %s (range 0-%u)", unparse_op().c_str(), data, (will_be ? "true" : "false"), this_mask);
	    _u.u = _mask.u = 0;
	    _op_negated = !will_be;
	    _op = OP_EQ;
	    return 0;
	}

	// value < X == !(value > (X - 1))
	if (_op == OP_LT) {
	    _u.u--;
	    _op_negated = !_op_negated;
	    _op = OP_GT;
	}

	_u.u = (_u.u << shift) | ((1 << shift) - 1);
	_mask.u = (this_mask << shift) | ((1 << shift) - 1);
	// Want (_u.u & _mask.u) == _u.u.
	// So change 'tcp[0] & 5 > 2' into the equivalent 'tcp[0] & 5 > 1':
	// find the highest bit in _u that is not set in _mask,
	// and turn on all lower bits.
	if ((_u.u & _mask.u) != _u.u) {
	    uint32_t full_mask_u = (full_mask << shift) | ((1 << shift) - 1);
	    uint32_t missing_bits = (_u.u & _mask.u) ^ (_u.u & full_mask_u);
	    uint32_t add_mask = 0xFFFFFFFFU >> ffs_msb(missing_bits);
	    _u.u = (_u.u | add_mask) & _mask.u;
	}
	return 0;
    }

    if (data > full_mask)
	return errh->error("%<%s%>: out of range, bound %u", unparse_type().c_str(), full_mask);

    _u.u = data << shift;
    _mask.u = this_mask << shift;
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
   case TYPE_NONE: sa << "<none>"; break;
   case TYPE_HOST: sa << "ip host"; break;
   case TYPE_PROTO: sa << "proto"; break;
   case TYPE_ETHER: sa << "ether host"; break;
   case TYPE_IPFRAG: sa << "ip frag"; break;
   case TYPE_PORT: sa << "port"; break;
   case TYPE_TCPOPT: sa << "tcp opt"; break;
   case TYPE_NET: sa << "ip net"; break;
   case TYPE_IPUNFRAG: sa << "ip unfrag"; break;
   case TYPE_IPECT: sa << "ip ect"; break;
   case TYPE_IPCE: sa << "ip ce"; break;
   default:
    if (type & TYPE_FIELD) {
      switch (type) {
       case FIELD_IPLEN: sa << "ip len"; break;
       case FIELD_ID: sa << "ip id"; break;
       case FIELD_VERSION: sa << "ip vers"; break;
       case FIELD_HL: sa << "ip hl"; break;
       case FIELD_TOS: sa << "ip tos"; break;
       case FIELD_DSCP: sa << "ip dscp"; break;
       case FIELD_TTL: sa << "ip ttl"; break;
       case FIELD_TCP_WIN: sa << "tcp win"; break;
       case FIELD_ICMP_TYPE: sa << "icmp type"; break;
       default:
	if (type & FIELD_PROTO_MASK)
	  sa << unparse_transp_proto((type & FIELD_PROTO_MASK) >> FIELD_PROTO_SHIFT);
	else
	  sa << "ip";
	sa << "[...]";
	break;
      }
    } else
      sa << "<unknown type " << type << ">";
    break;
  }

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
   case IP_PROTO_TRANSP: return "transp";
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
IPFilter::Primitive::type_error(ErrorHandler *errh, const char *msg) const
{
    return errh->error("%<%s%>: %s", unparse_type().c_str(), msg);
}

int
IPFilter::Primitive::check(const Primitive &prev_prim, int header,
			   int mask_dt, const PrimitiveData &mask,
			   ErrorHandler *errh)
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
	case TYPE_ETHER:
	    _type = _data;
	    if (!_srcdst)
		_srcdst = prev_prim._srcdst;
	    break;

	case TYPE_PROTO:
	    _type = TYPE_PROTO;
	    break;

	case TYPE_PORT:
	    _type = TYPE_PORT;
	    if (!_srcdst)
		_srcdst = prev_prim._srcdst;
	    if (_transp_proto == UNKNOWN)
		_transp_proto = prev_prim._transp_proto;
	    break;

	case TYPE_INT:
	    if (!(prev_prim._type & TYPE_FIELD)
		&& prev_prim._type != TYPE_PROTO
		&& prev_prim._type != TYPE_PORT)
		return errh->error("specify header field or %<port%>");
	    _data = prev_prim._type;
	    goto retry;

	case TYPE_NONE:
	    if (_transp_proto != UNKNOWN)
		_type = TYPE_PROTO;
	    else
		return errh->error("partial directive");
	    break;

	default:
	    if (_data & TYPE_FIELD) {
		_type = _data;
		if ((_type & FIELD_PROTO_MASK) && _transp_proto == UNKNOWN)
		    _transp_proto = (_type & FIELD_PROTO_MASK) >> FIELD_PROTO_SHIFT;
	    } else
		return errh->error("unknown type %<%s%>", unparse_type(0, _data).c_str());
	    break;

	}
    }

    // header can modify type
    if (header == 0 && _data == TYPE_ETHER)
	header = 'e';
    else if (header == 0)
	header = 'i';
    if (header == 'e' && _type == TYPE_HOST)
	_type = TYPE_ETHER;

    // check that _data and _type agree
    switch (_type) {

    case TYPE_HOST:
	if (header != 'i' || _data != TYPE_HOST)
	    return type_error(errh, "address missing");
	if (_op != OP_EQ)
	    goto operator_not_supported;
	_mask.u = 0xFFFFFFFFU;
	goto set_host_mask;

    case TYPE_NET:
	if (header != 'i' || _data != TYPE_NET)
	    return type_error(errh, "address missing");
	if (_op != OP_EQ)
	    goto operator_not_supported;
	_type = TYPE_HOST;
    set_host_mask:
	if (mask_dt && mask_dt != TYPE_INT && mask_dt != TYPE_HOST)
	    goto bad_mask;
	else if (mask_dt)
	    _mask.u = mask.u;
	break;

    case TYPE_ETHER:
	if (header != 'e' || _data != TYPE_ETHER)
	    return type_error(errh, "address missing");
	if (_op != OP_EQ)
	    goto operator_not_supported;
	memset(_mask.c, 0xFF, 6);
	memset(_mask.c + 6, 0, 2);
	if (mask_dt && mask_dt != TYPE_ETHER)
	    goto bad_mask;
	else if (mask_dt)
	    memcpy(_mask.c, mask.c, 6);
	break;

    case TYPE_PROTO:
	if (header != 'i')
	    goto ip_only;
	if (_data == TYPE_INT || _data == TYPE_PROTO) {
	    if (_transp_proto != UNKNOWN && _transp_proto != _u.i)
		return type_error(errh, "specified twice");
	    _data = TYPE_NONE;
	} else
	    _u.i = _transp_proto;
	_transp_proto = UNKNOWN;
	if (_data != TYPE_NONE || _u.i == UNKNOWN)
	    goto value_missing;
	if (_u.i >= 256) {
	    if (_op != OP_EQ || mask_dt)
		return errh->error("%<%s%>: operator or mask not supported", unparse_transp_proto(_u.i).c_str());
	    _mask.u = 0xFF;
	} else if (mask_dt && mask_dt != TYPE_INT)
	    goto bad_mask;
	else if (set_mask(0xFF, 0, mask_dt ? mask.u : 0, errh) < 0)
	    return -1;
	if (_op == OP_EQ && _mask.u == 0xFF && !_op_negated) // set _transp_proto if allowed
	    _transp_proto = _u.i;
	break;

    case TYPE_PORT:
	if (header != 'i')
	    goto ip_only;
	if (_data == TYPE_INT)
	    _data = TYPE_PORT;
	if (_data != TYPE_PORT)
	    goto value_missing;
	if (_transp_proto == UNKNOWN)
	    _transp_proto = IP_PROTO_TCP_OR_UDP;
	else if (_transp_proto != IP_PROTO_TCP && _transp_proto != IP_PROTO_UDP
		 && _transp_proto != IP_PROTO_TCP_OR_UDP
		 && _transp_proto != IP_PROTO_DCCP)
	    return errh->error("%<port%>: bad protocol %d", _transp_proto);
	else if (mask_dt && mask_dt != TYPE_INT)
	    goto bad_mask;
	if (set_mask(0xFFFF, 0, mask_dt ? mask.u : 0, errh) < 0)
	    return -1;
	break;

    case TYPE_TCPOPT:
	if (header != 'i')
	    goto ip_only;
	if (_data == TYPE_INT)
	    _data = TYPE_TCPOPT;
	if (_data != TYPE_TCPOPT)
	    goto value_missing;
	if (_transp_proto == UNKNOWN)
	    _transp_proto = IP_PROTO_TCP;
	else if (_transp_proto != IP_PROTO_TCP)
	    return errh->error("%<tcp opt%>: bad protocol %d", _transp_proto);
	if (_op != OP_EQ || _op_negated || mask_dt)
	    return type_error(errh, "operator or mask not supported");
	if (_u.i < 0 || _u.i > 255)
	    return errh->error("%<tcp opt%>: value %d out of range", _u.i);
	_mask.i = _u.i;
	break;

    case TYPE_IPECT:
	if (header != 'i')
	    goto ip_only;
	if (_data != TYPE_NONE && _data != TYPE_INT)
	    goto value_missing;
	if (_data == TYPE_NONE) {
	    _mask.u = IP_ECNMASK;
	    _u.u = 0;
	    _op_negated = true;
	} else if (mask_dt && mask_dt != TYPE_INT)
	    goto bad_mask;
	if (set_mask(0x3, 0, mask_dt ? mask.u : 0, errh) < 0)
	    return -1;
	_type = FIELD_TOS;
	break;

    case TYPE_IPCE:
	if (header != 'i')
	    goto ip_only;
	if (_data != TYPE_NONE || mask_dt)
	    goto value_not_supported;
	_mask.u = IP_ECNMASK;
	_u.u = IP_ECN_CE;
	_type = FIELD_TOS;
	break;

    case TYPE_IPFRAG:
	if (header != 'i')
	    goto ip_only;
	if (_data != TYPE_NONE || mask_dt)
	    goto value_not_supported;
	_mask.u = 1; // don't want mask to be 0
	break;

    case TYPE_IPUNFRAG:
	if (header != 'i')
	    goto ip_only;
	if (_data != TYPE_NONE || mask_dt)
	    goto value_not_supported;
	_op_negated = true;
	_mask.u = 1; // don't want mask to be 0
	_type = TYPE_IPFRAG;
	break;

    default:
	if (_type & TYPE_FIELD) {
	    if (header != 'i')
		goto ip_only;
	    if (_data != TYPE_INT && _data != _type)
		goto value_missing;
	    else if (mask_dt && mask_dt != TYPE_INT && mask_dt != _type)
		goto bad_mask;
	    int nbits = ((_type & FIELD_LENGTH_MASK) >> FIELD_LENGTH_SHIFT) + 1;
	    uint32_t xmask = (nbits == 32 ? 0xFFFFFFFFU : (1 << nbits) - 1);
	    if (set_mask(xmask, 0, mask_dt ? mask.u : 0, errh) < 0)
		return -1;
	}
	break;

    value_missing:
	return type_error(errh, "value missing");
    bad_mask:
	return type_error(errh, "bad mask");
    value_not_supported:
	return type_error(errh, "value not supported");
    operator_not_supported:
	return type_error(errh, "operator not supported");
    ip_only:
	return type_error(errh, "ip only");

    }

    // fix _srcdst
    if (_type == TYPE_HOST || _type == TYPE_PORT || _type == TYPE_ETHER) {
	if (_srcdst == 0)
	    _srcdst = SD_OR;
    } else if (old_srcdst)
	errh->warning("%<%s%>: %<src%> or %<dst%> ignored", unparse_type().c_str());

    return 0;
}

static void
add_exprs_for_proto(int32_t proto, int32_t mask, Classification::Wordwise::Program &p, Vector<int> &tree)
{
    if (mask == 0xFF && proto == IP_PROTO_TCP_OR_UDP) {
	p.start_subtree(tree);
	p.add_insn(tree, IPFilter::offset_net + 8, htonl(IP_PROTO_TCP << 16), htonl(0x00FF0000));
	p.add_insn(tree, IPFilter::offset_net + 8, htonl(IP_PROTO_UDP << 16), htonl(0x00FF0000));
	p.finish_subtree(tree, Classification::c_or);
    } else if (mask == 0xFF && proto >= 256)
	/* nada */;
    else
	p.add_insn(tree, IPFilter::offset_net + 8, htonl(proto << 16), htonl(mask << 16));
}

void
IPFilter::Primitive::add_comparison_exprs(Classification::Wordwise::Program &p, Vector<int> &tree, int offset, int shift, bool swapped, bool op_negate) const
{
  assert(_op == IPFilter::OP_EQ || _op == IPFilter::OP_GT);

  uint32_t mask = _mask.u;
  uint32_t u = _u.u & mask;
  if (swapped) {
    mask = ntohl(mask);
    u = ntohl(u);
  }

  if (_op == IPFilter::OP_EQ) {
    p.add_insn(tree, offset, htonl(u << shift), htonl(mask << shift));
    if (_op_negated && op_negate)
      p.negate_subtree(tree, true);
    return;
  }

  // To implement a greater-than test for "input&MASK > U":
  // Check the top bit of U&MASK.
  // If the top bit is 0, then:
  //    Find TOPMASK, the top bits of MASK s.t. U&TOPMASK == 0.
  //    If "input&TOPMASK == 0", continue testing with lower bits of
  //    U and MASK; combine with OR.
  //    Otherwise, succeed.
  // If the top bit is 1, then:
  //    Find TOPMASK, the top bits of MASK s.t. (U+1)&TOPMASK == TOPMASK.
  //    If "input&TOPMASK == TOPMASK", continue testing with lower bits of
  //    U and MASK; combine with AND.
  //    Otherwise, fail.
  // Stop testing when U >= MASK.

  int high_bit_record = 0;
  int count = 0;

  while (u < mask) {
    int high_bit = (u > (mask >> 1));
    int first_different_bit = 33 - ffs_msb(high_bit ? ~(u+1) & mask : u);
    uint32_t upper_mask;
    if (first_different_bit == 33)
      upper_mask = mask;
    else
      upper_mask = mask & ~((1 << first_different_bit) - 1);
    uint32_t upper_u = (high_bit ? 0xFFFFFFFF & upper_mask : 0);

    p.start_subtree(tree);
    p.add_insn(tree, offset, htonl(upper_u << shift), htonl(upper_mask << shift));
    if (!high_bit)
      p.negate_subtree(tree, true);
    high_bit_record = (high_bit_record << 1) | high_bit;
    count++;

    mask &= ~upper_mask;
    u &= mask;
  }

  while (count > 0) {
    p.finish_subtree(tree, (high_bit_record & 1 ? Classification::c_and : Classification::c_or));
    high_bit_record >>= 1;
    count--;
  }

  if (_op_negated && op_negate)
    p.negate_subtree(tree, true);
}

void
IPFilter::Primitive::compile(Classification::Wordwise::Program &p, Vector<int> &tree) const
{
  p.start_subtree(tree);

  // handle transport protocol uniformly
  if (_transp_proto != UNKNOWN)
    add_exprs_for_proto(_transp_proto, 0xFF, p, tree);

  // enforce first fragment: fragmentation offset == 0
  if (_type == TYPE_PORT || _type == TYPE_TCPOPT || ((_type & TYPE_FIELD) && (_type & FIELD_PROTO_MASK)))
    p.add_insn(tree, offset_net + 4, 0, htonl(0x00001FFF));

  // handle other types
  switch (_type) {

  case TYPE_HOST:
      p.start_subtree(tree);
      if (_srcdst == SD_SRC || _srcdst == SD_AND || _srcdst == SD_OR)
	  add_comparison_exprs(p, tree, offset_net + 12, 0, true, false);
      if (_srcdst == SD_DST || _srcdst == SD_AND || _srcdst == SD_OR)
	  add_comparison_exprs(p, tree, offset_net + 16, 0, true, false);
  finish_srcdst:
      p.finish_subtree(tree, (_srcdst == SD_OR ? Classification::c_or : Classification::c_and));
      if (_op_negated)
	  p.negate_subtree(tree, true);
      break;

  case TYPE_ETHER: {
      p.start_subtree(tree);
      Primitive copy(*this);
      if (_srcdst == SD_SRC || _srcdst == SD_AND || _srcdst == SD_OR) {
          p.start_subtree(tree);
	  memcpy(copy._u.c, _u.c, 4);
	  memcpy(copy._mask.c, _mask.c, 4);
	  copy.add_comparison_exprs(p, tree, offset_mac + 8, 0, true, false);
	  copy._u.u = copy._mask.u = 0;
	  memcpy(copy._u.c, _u.c + 4, 2);
	  memcpy(copy._mask.c, _mask.c + 4, 2);
	  copy.add_comparison_exprs(p, tree, offset_mac + 12, 0, true, false);
          p.finish_subtree(tree, Classification::c_and);
      }
      if (_srcdst == SD_DST || _srcdst == SD_AND || _srcdst == SD_OR) {
          p.start_subtree(tree);
	  copy._u.u = copy._mask.u = 0;
	  memcpy(copy._u.c + 2, _u.c, 2);
	  memcpy(copy._mask.c + 2, _mask.c, 2);
	  copy.add_comparison_exprs(p, tree, offset_mac, 0, true, false);
	  memcpy(copy._u.c, _u.c + 2, 4);
	  memcpy(copy._mask.c, _mask.c + 2, 4);
	  copy.add_comparison_exprs(p, tree, offset_mac + 4, 0, true, false);
          p.finish_subtree(tree, Classification::c_and);
      }
      goto finish_srcdst;
  }

  case TYPE_PROTO:
      if (_transp_proto < 256)
	  add_comparison_exprs(p, tree, offset_net + 8, 16, false, true);
      break;

  case TYPE_IPFRAG:
      p.add_insn(tree, offset_net + 4, 0, htonl(0x00003FFF));
      if (!_op_negated)
	  p.negate_subtree(tree, true);
      break;

  case TYPE_PORT:
      p.start_subtree(tree);
      if (_srcdst == SD_SRC || _srcdst == SD_AND || _srcdst == SD_OR)
	  add_comparison_exprs(p, tree, offset_transp, 16, false, false);
      if (_srcdst == SD_DST || _srcdst == SD_AND || _srcdst == SD_OR)
	  add_comparison_exprs(p, tree, offset_transp, 0, false, false);
      goto finish_srcdst;

  case TYPE_TCPOPT:
      p.add_insn(tree, offset_transp + 12, htonl(_u.u << 16), htonl(_mask.u << 16));
      break;

  default:
      if (_type & TYPE_FIELD) {
	  int offset = (_type & FIELD_OFFSET_MASK) >> FIELD_OFFSET_SHIFT;
	  int length = ((_type & FIELD_LENGTH_MASK) >> FIELD_LENGTH_SHIFT) + 1;
	  int word_offset = (offset >> 3) & ~3, bit_offset = offset & 0x1F;
	  int base_offset = (_type & FIELD_PROTO_MASK ? offset_transp : offset_net);
	  add_comparison_exprs(p, tree, base_offset + word_offset, 32 - (bit_offset + length), false, true);
      } else
	  assert(0);
      break;

  }

  p.finish_subtree(tree);
}


static void
separate_text(const String &text, Vector<String> &words)
{
  const char* s = text.data();
  int len = text.length();
  int pos = 0;
  while (pos < len) {
    while (pos < len && isspace((unsigned char) s[pos]))
      pos++;
    switch (s[pos]) {

     case '&': case '|':
      if (pos < len - 1 && s[pos+1] == s[pos])
	goto two_char;
      goto one_char;

     case '<': case '>': case '!': case '=':
      if (pos < len - 1 && s[pos+1] == '=')
	goto two_char;
      goto one_char;

     case '(': case ')': case '[': case ']': case ',': case ';':
     case '?':
     one_char:
      words.push_back(text.substring(pos, 1));
      pos++;
      break;

     two_char:
      words.push_back(text.substring(pos, 2));
      pos += 2;
      break;

     default: {
	int first = pos;
	while (pos < len && (isalnum((unsigned char) s[pos]) || s[pos] == '-' || s[pos] == '.' || s[pos] == '/' || s[pos] == '@' || s[pos] == '_' || s[pos] == ':'))
	  pos++;
	if (pos == first)
	  pos++;
	words.push_back(text.substring(first, pos - first));
	break;
      }

    }
  }
}

/*
 * expr ::= orexpr
 *	|   orexpr ? expr : expr
 * orexpr ::= orexpr || orexpr
 *	|   orexpr or orexpr
 *	|   term
 * term ::= term && term
 *	|   term and term
 *	|   term factor			// juxtaposition = and
 *	|   term
 * factor ::= ! factor
 *	|   ( expr )
 *	|   test
 * test ::= true
 *	|   false
 *	|   quals data
 *	|   quals relop data
 */

int
IPFilter::Parser::parse_expr_iterative(int pos)
{
    Vector<parse_state> stk;
    stk.push_back(parse_state(s_expr0));

    while (stk.size()) {
	parse_state &ps = stk.back();
	int new_state = -1;

	switch (ps.state) {
	case s_expr0:
	    _prog.start_subtree(_tree);
	    ps.state = s_expr1;
	    new_state = s_orexpr0;
	    break;
	case s_expr1:
	    if (pos >= _words.size() || _words[pos] != "?")
		goto finish_expr;
	    ++pos;
	    ps.state = s_expr2;
	    new_state = s_expr0;
	    break;
	case s_expr2:
	    if (pos == ps.last_pos || pos >= _words.size() || _words[pos] != ":") {
		_errh->error("missing %<:%> in ternary expression");
		goto finish_expr;
	    }
	    ++pos;
	    ps.state = s_expr1;
	    new_state = s_orexpr0;
	    break;
	finish_expr:
	    _prog.finish_subtree(_tree, Classification::c_ternary);
	    break;

	case s_orexpr0:
	    _prog.start_subtree(_tree);
	    ps.state = s_orexpr1;
	    new_state = s_term0;
	    break;
	case s_orexpr1:
	    if (pos >= _words.size() || (_words[pos] != "or" && _words[pos] != "||"))
		goto finish_orexpr;
	    ++pos;
	    new_state = s_term0;
	    break;
	finish_orexpr:
	    _prog.finish_subtree(_tree, Classification::c_or);
	    break;

	case s_term0:
	    _prog.start_subtree(_tree);
	    ps.state = s_term1;
	    new_state = s_factor0;
	    break;
	case s_term1:
	case s_term2:
	    if (pos == ps.last_pos) {
		if (ps.state == s_term1)
		    _errh->error("missing expression");
		goto finish_term;
	    }
	    if (pos < _words.size() && (_words[pos] == "and" || _words[pos] == "&&")) {
		ps.state = s_term1;
		++pos;
	    } else
		ps.state = s_term2;
	    new_state = s_factor0;
	    break;
	finish_term:
	    _prog.finish_subtree(_tree);
	    break;

	case s_factor0:
	case s_factor0_neg:
	    if (pos < _words.size() && (_words[pos] == "not" || _words[pos] == "!")) {
		ps.state += (s_factor1 - s_factor0);
		new_state = (ps.state == s_factor1 ? s_factor0_neg : s_factor0);
		++pos;
	    } else if (pos < _words.size() && _words[pos] == "(") {
		ps.state += (s_factor2 - s_factor0);
		new_state = s_expr0;
		++pos;
	    } else
		pos = parse_test(pos, ps.state == s_factor0_neg);
	    break;
	case s_factor1:
	case s_factor1_neg:
	    if (pos == ps.last_pos)
		_errh->error("missing expression after %<%s%>", _words[pos - 1].c_str());
	    break;
	case s_factor2:
	case s_factor2_neg:
	    if (pos == ps.last_pos)
		_errh->error("missing expression after %<(%>");
	    if (pos < _words.size() && _words[pos] == ")")
		++pos;
	    else if (pos != ps.last_pos)
		_errh->error("missing %<)%>");
	    if (ps.state == s_factor2_neg)
		_prog.negate_subtree(_tree);
	    break;
	}

	if (new_state >= 0) {
	    ps.last_pos = pos;
	    stk.push_back(parse_state(new_state));
	} else
	    stk.pop_back();
    }

    return pos;
}

static int
parse_brackets(IPFilter::Primitive& prim, const Vector<String>& words, int pos,
	       ErrorHandler* errh)
{
  int first_pos = pos + 1;
  String combination;
  for (pos++; pos < words.size() && words[pos] != "]"; pos++)
    combination += words[pos];
  if (pos >= words.size()) {
    errh->error("missing %<]%>");
    return first_pos;
  }
  pos++;

  // parse 'combination'
  int fieldpos, len = 1;
  const char* colon = find(combination.begin(), combination.end(), ':');
  const char* comma = find(combination.begin(), combination.end(), ',');
  if (colon < combination.end() - 1) {
    if (cp_integer(combination.begin(), colon, 0, &fieldpos) == colon
	&& cp_integer(colon + 1, combination.end(), 0, &len) == combination.end())
      goto non_syntax_error;
  } else if (comma < combination.end() - 1) {
    int pos2;
    if (cp_integer(combination.begin(), comma, 0, &fieldpos) == comma
	&& cp_integer(comma + 1, combination.end(), 0, &pos2) == combination.end()) {
      len = pos2 - fieldpos + 1;
      goto non_syntax_error;
    }
  } else if (IntArg().parse(combination, fieldpos))
    goto non_syntax_error;
  errh->error("syntax error after %<[%>, expected %<[POS]%> or %<[POS:LEN]%>");
  return pos;

 non_syntax_error:
  int multiplier = 8;
  fieldpos *= multiplier, len *= multiplier;
  if (len < 1 || len > 32)
    errh->error("LEN in %<[POS:LEN]%> out of range, should be between 1 and 4");
  else if ((fieldpos & ~31) != ((fieldpos + len - 1) & ~31))
      errh->error("field [%d:%d] does not fit in a single word", fieldpos/multiplier, len/multiplier);
  else {
    int transp = prim._transp_proto;
    if (transp == IPFilter::UNKNOWN)
      transp = 0;
    prim.set_type(IPFilter::TYPE_FIELD
		  | (transp << IPFilter::FIELD_PROTO_SHIFT)
		  | (fieldpos << IPFilter::FIELD_OFFSET_SHIFT)
		  | ((len - 1) << IPFilter::FIELD_LENGTH_SHIFT), errh);
  }
  return pos;
}

int
IPFilter::Parser::parse_test(int pos, bool negated)
{
    if (pos >= _words.size())
	return pos;
    String first_word = _words[pos];
    if (first_word == ")" || first_word == "||" || first_word == "or"
	|| first_word == "?" || first_word == ":")
	return pos;

    // 'true' and 'false'
    if (first_word == "true") {
	_prog.add_insn(_tree, 0, 0, 0);
	if (negated)
	    _prog.negate_subtree(_tree);
	return pos + 1;
    }
    if (first_word == "false") {
	_prog.add_insn(_tree, 0, 0, 0);
	if (!negated)
	    _prog.negate_subtree(_tree);
	return pos + 1;
    }

    // hard case

    // expect quals [relop] data
    int first_pos = pos;
    Primitive prim;
    int header = 0;

    // collect qualifiers
    for (; pos < _words.size(); pos++) {
	String wd = _words[pos];
	uint32_t wdata;
	int wt = lookup(wd, 0, UNKNOWN, wdata, _context, 0);

	if (wt >= 0 && wt == TYPE_TYPE) {
	    prim.set_type(wdata, _errh);
	    if ((wdata & TYPE_FIELD) && (wdata & FIELD_PROTO_MASK))
		prim.set_transp_proto((wdata & FIELD_PROTO_MASK) >> FIELD_PROTO_SHIFT, _errh);

	} else if (wt >= 0 && wt == TYPE_PROTO)
	    prim.set_transp_proto(wdata, _errh);

	else if (wt != -1)
	    break;

	else if (wd == "src") {
	    if (pos < _words.size() - 2 && (_words[pos+2] == "dst" || _words[pos+2] == "dest")) {
		if (_words[pos+1] == "and" || _words[pos+1] == "&&") {
		    prim.set_srcdst(SD_AND, _errh);
		    pos += 2;
		} else if (_words[pos+1] == "or" || _words[pos+1] == "||") {
		    prim.set_srcdst(SD_OR, _errh);
		    pos += 2;
		} else
		    prim.set_srcdst(SD_SRC, _errh);
	    } else
		prim.set_srcdst(SD_SRC, _errh);
	} else if (wd == "dst" || wd == "dest")
	    prim.set_srcdst(SD_DST, _errh);

	else if (wd == "ip" || wd == "ether") {
	    if (header)
		break;
	    header = wd[0];

	} else if (wd == "not" || wd == "!")
	    negated = !negated;

	else
	    break;
    }

    // prev_prim is not relevant if there were any qualifiers
    if (pos != first_pos)
	_prev_prim.clear();
    if (_prev_prim._data == TYPE_ETHER)
	header = 'e';

    // optional [] syntax
    String wd = (pos >= _words.size() - 1 ? String() : _words[pos]);
    if (wd == "[" && pos > first_pos && prim._type == TYPE_NONE) {
	pos = parse_brackets(prim, _words, pos, _errh);
	wd = (pos >= _words.size() - 1 ? String() : _words[pos]);
    }

    // optional bitmask
    int mask_dt = 0;
    PrimitiveData provided_mask;
    if (wd == "&" && pos < _words.size() - 1) {
	if (IntArg().parse(_words[pos + 1], provided_mask.u)) {
	    mask_dt = TYPE_INT;
	    pos += 2;
	} else if (header != 'e' && IPAddressArg().parse(_words[pos + 1], provided_mask.ip4, _context)) {
	    mask_dt = TYPE_HOST;
	    pos += 2;
	} else if (header != 'i' && EtherAddressArg().parse(_words[pos + 1], provided_mask.c, _context)) {
	    mask_dt = TYPE_ETHER;
	    pos += 2;
	}
	if (mask_dt && mask_dt != TYPE_ETHER && !provided_mask.u) {
	    _errh->error("zero mask ignored");
	    mask_dt = 0;
	}
	wd = (pos >= _words.size() - 1 ? String() : _words[pos]);
    }

    // optional relational operation
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
    if (pos < _words.size()) {
	wd = _words[pos];
	uint32_t wdata;
	int wt = lookup(wd, prim._type, prim._transp_proto, wdata, _context, _errh);
	pos++;

	if (wt == -2)		// ambiguous or incorrect word type
	    /* absorb word, but do nothing */
	    prim._type = -2;

	else if (wt != -1 && wt != TYPE_TYPE) {
	    prim._data = wt;
	    prim._u.u = wdata;

	} else if (IntArg().parse(wd, prim._u.i))
	    prim._data = TYPE_INT;

	else if (header != 'e' && IPAddressArg().parse(wd, prim._u.ip4, _context)) {
	    if (pos < _words.size() - 1 && _words[pos] == "mask"
		&& IPAddressArg().parse(_words[pos+1], prim._mask.ip4, _context)) {
		pos += 2;
		prim._data = TYPE_NET;
	    } else if (prim._type == TYPE_NET && IPPrefixArg().parse(wd, prim._u.ip4, prim._mask.ip4, _context))
		prim._data = TYPE_NET;
	    else
		prim._data = TYPE_HOST;

	} else if (header != 'e' && IPPrefixArg().parse(wd, prim._u.ip4, prim._mask.ip4, _context))
	    prim._data = TYPE_NET;

	else if (header != 'i' && EtherAddressArg().parse(wd, prim._u.c, _context))
	    prim._data = TYPE_ETHER;

	else {
	    if (prim._op != OP_EQ || prim._op_negated)
		_errh->error("dangling operator near %<%s%>", wd.c_str());
	    pos--;
	}
    }

    if (pos == first_pos) {
	_errh->error("empty term near %<%s%>", wd.c_str());
	return pos;
    }

    // add if it is valid
    if (prim.check(_prev_prim, header, mask_dt, provided_mask, _errh) >= 0) {
	prim.compile(_prog, _tree);
	if (negated)
	    _prog.negate_subtree(_tree);
	_prev_prim = prim;
    }

    return pos;
}

void
merge_programs(Classification::Wordwise::Program &p1, Classification::Wordwise::Program &p2)
{
    int step_offset = p1.ninsn();
    int jump_target = step_offset;
    if (p1.ninsn() == 0) {
        // p1 program is empty.
        // If it has a valid target, we always jump to it. p2 is ignored.
        // If target is invalid, proceed as normal. Resulting tree == p2.
        int target = -p1.output_everything();

        //check if target is valid
        if (target != Classification::j_never + 1) {
            return;
        }
    }
    if (p2.ninsn() == 0) {
        jump_target = -p2.output_everything();
    }

    p1.redirect_unfinished_insn_tree(jump_target);
    p2.offset_insn_tree(step_offset);

    for (int i = 0; i < p2.ninsn(); i++) {
       p1.add_raw_insn(p2.insn(i));
    }
}

void
IPFilter::parse_program(Classification::Wordwise::CompressedProgram &zprog,
			const Vector<String> &conf, int noutputs,
			const Element *context, ErrorHandler *errh)
{
    static const int offset_map[] = { offset_net + 8, offset_net + 3 };
    Vector<Classification::Wordwise::Program> progs;

    // [QUALS] [host|net|port|proto] [data]
    // QUALS ::= src | dst | src and dst | src or dst | \empty
    //        |  ip | icmp | tcp | udp
    for (int argno = 0; argno < conf.size(); argno++) {
	Vector<String> words;
	separate_text(cp_unquote(conf[argno]), words);

	if (words.size() == 0) {
	    errh->error("empty pattern %d", argno);
	    continue;
	}

	PrefixErrorHandler cerrh(errh, "pattern " + String(argno) + ": ");

	// get slot
	int slot = -Classification::j_never;
	{
	    String slotwd = words[0];
	    if (slotwd == "allow") {
		slot = 0;
		if (noutputs == 0)
		    cerrh.error("%<allow%> is meaningless, element has zero outputs");
	    } else if (slotwd == "deny") {
		if (noutputs > 1)
		    cerrh.warning("meaning of %<deny%> has changed (now it means %<drop%>)");
	    } else if (slotwd == "drop")
		/* nada */;
	    else if (IntArg().parse(slotwd, slot)) {
		if (slot < 0 || slot >= noutputs) {
		    cerrh.error("slot %<%d%> out of range", slot);
		    slot = -Classification::j_never;
		}
	    } else
		cerrh.error("unknown slot ID %<%s%>", slotwd.c_str());
	}

	Classification::Wordwise::Program prog;
	Vector<int> tree = prog.init_subtree();
	prog.start_subtree(tree);

	// check for "-"
	if (words.size() == 1
	    || (words.size() == 2
		&& (words[1] == "-" || words[1] == "any" || words[1] == "all")))
	    prog.add_insn(tree, 0, 0, 0);
	else {
	    Parser parser(words, tree, prog, context, &cerrh);
	    int pos = parser.parse_expr_iterative(1);
	    if (pos < words.size())
		cerrh.error("garbage after expression at %<%s%>", words[pos].c_str());
	}

	prog.finish_subtree(tree, Classification::c_and, -slot);
	progs.push_back(prog);
    }

    int num_progs = progs.size();
    if (num_progs == 1) {
        progs[0].optimize(offset_map, offset_map + 2, Classification::offset_max);
    } else {
        //recursivly merge programs in pairs of two
        int n = 1;
        while (n * 2 <= num_progs) {
            for (int i = 0; i < num_progs; i += n * 2) {
                int a = i;
                int b = i + n;
                if (b < num_progs) {
                    merge_programs(progs[a], progs[b]);
                    progs[a].optimize(offset_map, offset_map + 2, Classification::offset_max);
                } else {
                    //odd prog at end, not merging yet
                    continue;
                }
            }
            n = n * 2;
        }

        //check if there is an odd program at the end not yet merged
        if (n != num_progs) {
            merge_programs(progs[0], progs[n]);
            progs[0].optimize(offset_map, offset_map + 2, Classification::offset_max);
        }
    }

    // Compress the program into _zprog.
    // It helps to do another bubblesort for things like ports.
    progs[0].bubble_sort_and_exprs(offset_map, offset_map + 2, Classification::offset_max);
    zprog.compile(progs[0], PERFORM_BINARY_SEARCH, MIN_BINARY_SEARCH);

    // click_chatter("%s", zprog.unparse().c_str());
}

int
IPFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    IPFilterProgram zprog;
    parse_program(zprog, conf, noutputs(), this, errh);
    if (!errh->nerrors()) {
	_zprog = zprog;
	return 0;
    } else
	return -1;
}

String
IPFilter::program_string(Element *e, void *)
{
    IPFilter *ipf = static_cast<IPFilter *>(e);
    return ipf->_zprog.unparse();
}

void
IPFilter::add_handlers()
{
    add_read_handler("program", program_string);
}


//
// RUNNING
//

int
IPFilter::length_checked_match(const IPFilterProgram &zprog, const Packet *p,
			       int packet_length)
{
    const unsigned char *neth_data = p->network_header();
    const unsigned char *transph_data = p->transport_header();
    const uint32_t *pr = zprog.begin();
    const uint32_t *pp;
    uint32_t data = 0;

    while (1) {
	int off = (int16_t) pr[0];
	if (off + 4 > packet_length)
	    goto check_length;

    length_ok:
	if (off >= offset_transp)
	    data = *(const uint32_t *)(transph_data + off - offset_transp);
	else if (off >= offset_net)
	    data = *(const uint32_t *)(neth_data + off - offset_net);
	else
	    data = *(const uint32_t *)(p->mac_header() - 2 + off);
	data &= pr[3];
	off = pr[0] >> 17;
	pp = pr + 4;
	if (!PERFORM_BINARY_SEARCH || off < MIN_BINARY_SEARCH) {
	    for (; off; --off, ++pp)
		if (*pp == data) {
		    off = pr[2];
		    goto gotit;
		}
	} else {
	    const uint32_t *px = pp + off;
	    while (pp < px) {
		const uint32_t *pm = pp + (px - pp) / 2;
		if (*pm == data) {
		    off = pr[2];
		    goto gotit;
		} else if (*pm < data)
		    pp = pm + 1;
		else
		    px = pm;
	    }
	}
	off = pr[1];
    gotit:
	if (off <= 0)
	    return -off;
	pr += off;
	continue;

    check_length:
	if (off < packet_length) {
	    unsigned available = packet_length - off;
	    const uint8_t *c = (const uint8_t *) &pr[3];
	    if (!(c[3]
		  || (c[2] && available <= 2)
		  || (c[1] && available == 1)))
		goto length_ok;
	}
	off = pr[1 + ((pr[0] & 0x10000) != 0)];
	goto gotit;
    }
}

void
IPFilter::push(int, Packet *p)
{
    checked_output_push(match(_zprog, p), p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Classification)
EXPORT_ELEMENT(IPFilter)
