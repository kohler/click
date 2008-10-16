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
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/integers.hh>
CLICK_DECLS

/** @file ipaddress.hh
 * @brief The IPAddress type for IPv4 addresses.
 */

/** @class IPAddress
    @brief An IPv4 address.

    The IPAddress type represents an IPv4 address. It supports bitwise
    operations like & and | and provides methods for unparsing IP addresses
    into ASCII dotted-quad form. */

/** @brief Constructs an IPAddress from data.
    @param data the address data, in network byte order

    The bytes data[0]...data[3] are used to construct the address. */
IPAddress::IPAddress(const unsigned char *data)
{
#ifdef HAVE_INDIFFERENT_ALIGNMENT
    _addr = *(reinterpret_cast<const unsigned *>(data));
#else
    memcpy(&_addr, data, 4);
#endif
}

/** @brief Constructs an IPAddress from a human-readable dotted-quad
    representation.
    @param str the unparsed address

    If @a str is not a valid dotted-quad address, then the IPAddress is
    initialized to 0.0.0.0. */
IPAddress::IPAddress(const String &str)
{
    if (!cp_ip_address(str, this))
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

CLICK_ENDDECLS
