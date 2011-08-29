// -*- c-basic-offset: 4; related-file-name: "../include/click/ipaddress.hh" -*-
/*
 * ipaddress.{cc,hh} -- an IP address class
 * Robert Morris / John Jannotti / Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/integers.hh>
#if !CLICK_TOOL
# include <click/nameinfo.hh>
# include <click/standard/addressinfo.hh>
#elif HAVE_NETDB_H
# include <netdb.h>
#endif
CLICK_DECLS

/** @file ipaddress.hh
 * @brief The IPAddress type for IPv4 addresses.
 */

/** @class IPAddress
    @brief An IPv4 address.

    The IPAddress type represents an IPv4 address. It supports bitwise
    operations like & and | and provides methods for unparsing IP addresses
    into ASCII dotted-quad form. */

IPAddress::IPAddress(const String &str)
{
    if (!IPAddressArg::parse(str, *this))
	_addr = 0;
}

IPAddress
IPAddress::make_prefix(int prefix_len)
{
    assert(prefix_len >= 0 && prefix_len <= 32);
    uint32_t umask = 0;
    if (prefix_len > 0)
	umask = 0xFFFFFFFFU << (32 - prefix_len);
    return IPAddress(htonl(umask));
}

/** @brief Returns the prefix length equivalent to this prefix mask, or -1 if
    this is not a prefix mask.

    Maintains the invariant that make_prefix(@a
    prefix_len).mask_to_prefix_len() == @a prefix_len.
    @sa make_prefix */
int
IPAddress::mask_to_prefix_len() const
{
    uint32_t host_addr = ntohl(_addr);
    int prefix = ffs_lsb(host_addr);
    if (prefix == 0)
	return 0;
    else if (host_addr == (0xFFFFFFFFU << (prefix - 1)))
	return 33 - prefix;
    else
	return -1;
}

/** @brief Unparses this address into a dotted-quad format String.

    Examples include "0.0.0.0" and "18.26.4.9".  Maintains the invariant that,
    for an IPAddress @a a, IPAddress(@a a.unparse()) == @a a. */
String
IPAddress::unparse() const
{
    const unsigned char *p = data();
    char buf[20];
    sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
    return String(buf);
}

/** @brief Unparses this address into IP address mask format: either a prefix
    length or unparse().

    If mask_to_prefix_len() >= 0, then returns that value as a base-10 String;
    otherwise, returns unparse().  Example results include "8" (for 255.0.0.0)
    and "18.26.4.9". */
String
IPAddress::unparse_mask() const
{
    int prefix_len = mask_to_prefix_len();
    if (prefix_len >= 0)
	return String(prefix_len);
    else
	return unparse();
}

/** @brief Unparses an address prefix, specified by address and mask, into
    "address/mask" format.
    @param mask the address mask

    Equivalent to unparse() + "/" + @a mask.unparse_mask(). */
String
IPAddress::unparse_with_mask(IPAddress mask) const
{
    return unparse() + "/" + mask.unparse_mask();
}

StringAccum &
operator<<(StringAccum &sa, IPAddress ipa)
{
    const unsigned char *p = ipa.data();
    char buf[20];
    int amt;
    amt = sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
    sa.append(buf, amt);
    return sa;
}


const char *
IPAddressArg::basic_parse(const char *s, const char *end,
			  unsigned char value[4], int &nbytes)
{
    memset(value, 0, 4);
    int d = 0;
    while (d < 4 && s != end && (d == 0 || *s == '.')) {
	const char *t = s + !!d;
	if (t == end || !isdigit((unsigned char) *t))
	    break;
	int part = *t - '0';
	for (++t; t != end && isdigit((unsigned char) *t) && part <= 255; ++t)
	    part = part * 10 + *t - '0';
	if (part > 255)
	    break;
	s = t;
	value[d] = part;
	if (++d == 4)
	    break;
    }
    nbytes = d;
    return s;
}

bool
IPAddressArg::parse(const String &str, IPAddress &result, const ArgContext &args)
{
    unsigned char value[4];
    int nbytes;
    if (basic_parse(str.begin(), str.end(), value, nbytes) == str.end()
	&& nbytes == 4) {
	memcpy(&result, value, 4);
	return true;
    }
#if !CLICK_TOOL
    return AddressInfo::query_ip(str, result.data(), args.context());
#else
    (void) args;
    return false;
#endif
}

bool
IPAddressArg::parse(const String &str, Vector<IPAddress> &result, const ArgContext &args)
{
    Vector<IPAddress> v;
    String arg(str);
    IPAddress ip;
    int nwords = 0;
    while (String word = cp_shift_spacevec(arg)) {
	++nwords;
	if (parse(word, ip, args))
	    v.push_back(ip);
	else
	    return false;
    }
    if (nwords == v.size()) {
	v.swap(result);
	return true;
    }
    args.error("out of memory");
    return false;
}

static bool
prefix_with_slash(const String &str, const char *slash,
		  IPAddress &result_addr, IPAddress &result_mask,
		  const ArgContext &args)
{
    unsigned char addr[4];
    int nbytes;
    if (IPAddressArg::basic_parse(str.begin(), slash, addr, nbytes) == slash)
	/* nada */;
#if !CLICK_TOOL
    else if (args.context()
	     && AddressInfo::query_ip(str.substring(str.begin(), slash), addr, args.context()))
	nbytes = 4;
#endif
    else
	return false;

    int l = -1;
    IPAddress mask;
    if (IntArg(10).parse(str.substring(slash + 1, str.end()), l)
	&& l >= 0 && l <= 32)
	mask = IPAddress::make_prefix(l);
    else if (!IPAddressArg::parse(str.substring(slash + 1, str.end()), mask, args))
	return false;

    // check whether mask specifies more bits than user bothered to define
    // in the IP address, which is considered an error
    if (nbytes < 4 && (mask & IPAddress::make_prefix(nbytes * 8)) != mask)
	return false;

    memcpy(result_addr.data(), addr, 4);
    result_mask = mask;
    return true;
}

bool
IPPrefixArg::parse(const String &str,
		   IPAddress &result_addr, IPAddress &result_mask,
		   const ArgContext &args) const
{
    const char *begin = str.begin(), *slash = str.end(), *end = str.end();
    while (slash != begin)
	if (*--slash == '/')
	    break;

    if (slash != begin && slash + 1 != end
	&& prefix_with_slash(str, slash, result_addr, result_mask, args))
	return true;

    if (allow_bare_address && IPAddressArg::parse(str, result_addr, args)) {
	result_mask = 0xFFFFFFFFU;
	return true;
    }

#if !CLICK_TOOL
    return AddressInfo::query_ip_prefix(str, result_addr.data(),
					result_mask.data(), args.context());
#else
    return false;
#endif
}

bool
IPPortArg::parse(const String &str, uint16_t &result, const ArgContext &args) const
{
    uint32_t value;
#ifndef CLICK_TOOL
    if (!NameInfo::query_int(NameInfo::T_IP_PORT + ip_p, args.context(),
			     str, &value))
	return false;
#else
    if (!IntArg().parse(str, value)) {
# if HAVE_NETDB_H
	const char *proto_name;
	if (ip_p == IP_PROTO_TCP)
	    proto_name = "tcp";
	else if (ip_p == IP_PROTO_UDP)
	    proto_name = "udp";
	else if (ip_p == IP_PROTO_SCTP)
	    proto_name = "sctp";
	else if (ip_p == IP_PROTO_DCCP)
	    proto_name = "dccp";
	else
	    return false;
	if (struct servent *s = getservbyname(str.c_str(), proto_name)) {
	    result = ntohs(s->s_port);
	    return true;
	}
# endif
	return false;
    }
#endif
    if (value > 0xFFFF) {
	args.error("overflow, range 0-65535");
	return false;
    }
    result = value;
    return true;
}

CLICK_ENDDECLS
