// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * iprwpattern.{cc,hh} -- helper class for IPRewriter
 * Eddie Kohler
 * original version by Max Poletto and Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2008-2010 Meraki, Inc.
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
#include "iprwpattern.hh"
#include "elements/ip/iprwmapping.hh"
#include "elements/ip/iprwpatterns.hh"
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/confparse.hh>
#include <click/algorithm.hh>
#include <click/router.hh>
#include <click/nameinfo.hh>
#include <click/straccum.hh>
#include <click/error.hh>
CLICK_DECLS

IPRewriterPattern::IPRewriterPattern(const IPAddress &saddr, int sport,
		       const IPAddress &daddr, int dport,
		       bool is_napt, bool sequential, bool same_first,
		       uint32_t variation_top)
    : _saddr(saddr), _sport(sport), _daddr(daddr), _dport(dport),
      _variation_top(variation_top), _next_variation(0), _is_napt(is_napt),
      _sequential(sequential), _same_first(same_first), _refcount(0)
{
}

namespace {
enum { PE_SYNTAX, PE_NOPATTERN, PE_SADDR, PE_SPORT, PE_DADDR, PE_DPORT };
static const char* const pe_messages[] = {
    "syntax error",
    "no such pattern",
    "bad source address",
    "bad source port",
    "bad destination address",
    "bad destination port"
};
static inline bool
pattern_error(int what, ErrorHandler* errh)
{
    return errh->error(pe_messages[what]), false;
}

static bool
ip_address_variation(const String &str, IPAddress *addr, int32_t *variation,
		     bool *sequential, bool *same_first, Element *context)
{
    const char *end = str.end();
    if (end > str.begin() && end[-1] == '#')
	*sequential = true, *same_first = false, --end;
    else if (end > str.begin() && end[-1] == '?')
	*same_first = false, --end;
    const char *dash = find(str.begin(), end, '-');
    IPAddress addr2;

    if (dash != end
	&& IPAddressArg().parse(str.substring(str.begin(), dash), *addr, context)
	&& IPAddressArg().parse(str.substring(dash + 1, end), addr2, context)
	&& ntohl(addr2.addr()) >= ntohl(addr->addr())) {
	*variation = ntohl(addr2.addr()) - ntohl(addr->addr());
	return true;
    } else if (dash == end
	       && IPPrefixArg(true).parse(str.substring(str.begin(), end), *addr, addr2, context)
	       && *addr && addr2 && addr2.mask_to_prefix_len() >= 0) {
	if (addr2.addr() == 0xFFFFFFFFU)
	    *variation = 0;
	else if (addr2.addr() == htonl(0xFFFFFFFEU)) {
	    *addr &= addr2;
	    *variation = 1;
	} else {
	    // don't count PREFIX.0 and PREFIX.255
	    *addr = (*addr & addr2) | IPAddress(htonl(1));
	    *variation = ~ntohl(addr2.addr()) - 2;
	}
	return true;
    } else
	return false;
}

static bool
port_variation(const String &str, int32_t *port, int32_t *variation,
	       bool *sequential, bool *same_first)
{
    const char *end = str.end();
    if (end > str.begin() && end[-1] == '#')
	*sequential = true, *same_first = false, --end;
    else if (end > str.begin() && end[-1] == '?')
	*same_first = false, --end;
    const char *dash = find(str.begin(), end, '-');
    int32_t port2 = 0;

    if (IntArg().parse(str.substring(str.begin(), dash), *port)
	&& IntArg().parse(str.substring(dash + 1, end), port2)
	&& *port >= 0 && port2 >= *port && port2 < 65536) {
	*variation = port2 - *port;
	return true;
    } else
	return false;
}
}

bool
IPRewriterPattern::parse(const Vector<String> &words,
			 IPRewriterPattern **pstore,
			 Element *context, ErrorHandler *errh)
{
    if (words.size() < 1 || words.size() > 4)
	return pattern_error(PE_SYNTAX, errh);

    if (words.size() == 1) {
	int32_t x;
	Vector<IPRewriterPattern *> *patterns =
	    static_cast<Vector<IPRewriterPattern *> *>(context->router()->attachment("IPRewriterPatterns"));
	if (NameInfo::query_int(NameInfo::T_IPREWRITER_PATTERN, context,
				words[0], &x)
	    && patterns && x >= 0 && x < patterns->size()) {
	    *pstore = (*patterns)[x];
	    return true;
	} else {
	    errh->error("no such pattern %<%s%>", words[0].c_str());
	    return false;
	}
    }

    IPAddress saddr, daddr;
    int32_t sport = 0, dport = 0, variation = 0;
    bool sequential = false, same_first = true;

    // source address
    int i = 0;
    if (!(words[i].equals("-", 1)
	  || IPAddressArg().parse(words[i], saddr, context)
	  || ip_address_variation(words[i], &saddr, &variation,
				  &sequential, &same_first, context)))
	return pattern_error(PE_SADDR, errh);

    // source port
    if (words.size() >= 3) {
	i = words.size() == 3 ? 2 : 1;
	if (!(words[i].equals("-", 1)
	      || (IntArg().parse(words[i], sport) && sport > 0 && sport < 65536)
	      || port_variation(words[i], &sport, &variation,
				&sequential, &same_first)))
	    return pattern_error(PE_SPORT, errh);
	i = words.size() == 3 ? 0 : 1;
    }

    // destination address
    ++i;
    if (!(words[i].equals("-", 1)
	  || IPAddressArg().parse(words[i], daddr, context)))
	return pattern_error(PE_DADDR, errh);

    // destination port
    if (words.size() == 4) {
	++i;
	if (!(words[i].equals("-", 1)
	      || (IntArg().parse(words[i], dport) && dport > 0 && dport < 65536)))
	    return pattern_error(PE_DPORT, errh);
    }

    *pstore = new IPRewriterPattern(saddr, htons(sport), daddr, htons(dport),
				    words.size() >= 3,
				    sequential, same_first, variation);
    return true;
}

bool
IPRewriterPattern::parse_ports(const Vector<String> &words,
			       IPRewriterInput *input,
			       Element *, ErrorHandler *errh)
{
    if (!(words.size() == 2
	  && IntArg().parse(words[0], input->foutput)))
	return errh->error("bad forward port"), false;
    if (IntArg().parse(words[1], input->routput))
	return true;
    else
	return errh->error("bad reply port"), false;
}

bool
IPRewriterPattern::parse_with_ports(const String &conf, IPRewriterInput *input,
				    Element *e, ErrorHandler *errh)
{
    Vector<String> words, port_words;
    cp_spacevec(conf, words);

    if (words.size() <= 2)
	return pattern_error(PE_SYNTAX, errh);

    port_words.push_back(words[words.size() - 2]);
    port_words.push_back(words[words.size() - 1]);
    words.resize(words.size() - 2);

    return parse(words, &input->u.pattern, e, errh)
	&& parse_ports(port_words, input, e, errh);
}

int
IPRewriterPattern::rewrite_flowid(const IPFlowID &flowid,
				  IPFlowID &rewritten_flowid,
				  const HashContainer<IPRewriterEntry> &reply_map)
{
    rewritten_flowid = flowid;
    if (_saddr)
	rewritten_flowid.set_saddr(_saddr);
    if (_sport)
	rewritten_flowid.set_sport(_sport);
    if (_daddr)
	rewritten_flowid.set_daddr(_daddr);
    if (_dport)
	rewritten_flowid.set_dport(_dport);

    if (_variation_top) {
	IPFlowID lookup = rewritten_flowid.reverse();
	uint32_t base = (_is_napt ? ntohs(_sport) : ntohl(_saddr.addr()));

	uint32_t val;
	if (_same_first
	    && (val = ntohs(flowid.sport()) - base) <= _variation_top) {
	    lookup.set_dport(flowid.sport());
	    if (!reply_map.find(lookup))
		goto found_variation;
	}

	if (_sequential)
	    val = (_next_variation > _variation_top ? 0 : _next_variation);
	else
	    val = click_random(0, _variation_top);

	for (uint32_t count = 0; count <= _variation_top;
	     ++count, val = (val == _variation_top ? 0 : val + 1)) {
	    if (_is_napt)
		lookup.set_dport(htons(base + val));
	    else
		lookup.set_daddr(htonl(base + val));
	    if (!reply_map.find(lookup))
		goto found_variation;
	}

	return IPRewriterBase::rw_drop;

    found_variation:
	if (_is_napt)
	    rewritten_flowid.set_sport(lookup.dport());
	else
	    rewritten_flowid.set_saddr(lookup.daddr());
	_next_variation = val + 1;
    }

    return IPRewriterBase::rw_addmap;
}

String
IPRewriterPattern::unparse() const
{
    StringAccum sa;
    if (!_is_napt && _variation_top)
	sa << _saddr << '-'
	   << IPAddress(htonl(ntohl(_saddr.addr()) + _variation_top));
    else if (_saddr)
	sa << _saddr;
    else
	sa << '-';

    if (!_is_napt)
	/* nada */;
    else if (!_sport)
	sa << " -";
    else if (_variation_top)
	sa << ' ' << ntohs(_sport) << '-' << (ntohs(_sport) + _variation_top);
    else
	sa << ' ' << ntohs(_sport);

    if (_daddr)
	sa << ' ' << _daddr;
    else
	sa << " -";

    if (!_is_napt)
	/* nada */;
    else if (!_dport)
	sa << " -";
    else
	sa << ' ' << ntohs(_dport);

    return sa.take_string();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRewriterMapping)
ELEMENT_PROVIDES(IPRewriterPattern)
