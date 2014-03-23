// -*- related-file-name: "../include/click/etheraddress.hh" -*-
/*
 * etheraddress.{cc,hh} -- an Ethernet address class
 * Robert Morris / John Jannotti, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2006 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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
#include <click/etheraddress.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#if !CLICK_TOOL
# include <click/nameinfo.hh>
# include <click/standard/addressinfo.hh>
#endif
CLICK_DECLS

/** @file etheraddress.hh
 * @brief The EtherAddress type for Ethernet addresses.
 */

/** @class EtherAddress
    @brief An Ethernet address.

    The EtherAddress type represents an Ethernet address. It supports equality
    operations and provides methods for unparsing addresses into ASCII form. */

String
EtherAddress::unparse_dash() const
{
    static_assert(sizeof(EtherAddress) == 6, "EtherAddress has the wrong size.");
#if __GNUC__
    static_assert(__alignof__(EtherAddress) <= 2, "EtherAddress has unexpectedly strict alignment requirements.");
#endif

    String str = String::make_uninitialized(17);
    // NB: mutable_c_str() creates space for the terminating null character
    if (char *x = str.mutable_c_str()) {
	const unsigned char *p = this->data();
	sprintf(x, "%02X-%02X-%02X-%02X-%02X-%02X",
		p[0], p[1], p[2], p[3], p[4], p[5]);
    }
    return str;
}

String
EtherAddress::unparse_colon() const
{
    String str = String::make_uninitialized(17);
    if (char *x = str.mutable_c_str()) {
	const unsigned char *p = this->data();
	sprintf(x, "%02X:%02X:%02X:%02X:%02X:%02X",
		p[0], p[1], p[2], p[3], p[4], p[5]);
    }
    return str;
}

StringAccum &
operator<<(StringAccum &sa, const EtherAddress &ea)
{
    if (char *x = sa.extend(17, 1)) {
	const unsigned char *p = ea.data();
	sprintf(x, "%02X-%02X-%02X-%02X-%02X-%02X",
		p[0], p[1], p[2], p[3], p[4], p[5]);
    }
    return sa;
}


bool
EtherAddressArg::parse(const String& str, EtherAddress& value, const ArgContext& args,
                       int flags)
{
    unsigned char data[6];
    int d = 0, p = 0, sep = 0;
    const char *s, *end = str.end();

    for (s = str.begin(); s != end; ++s) {
	int digit;
	if (*s >= '0' && *s <= '9')
	    digit = *s - '0';
	else if (*s >= 'a' && *s <= 'f')
	    digit = *s - 'a' + 10;
	else if (*s >= 'A' && *s <= 'F')
	    digit = *s - 'A' + 10;
	else {
	    if (sep == 0 && (*s == '-' || *s == ':'))
		sep = *s;
	    if (*s == sep && (p == 1 || p == 2) && d < 5) {
		p = 0;
		++d;
		continue;
	    } else
		break;
	}

	if (p == 2 || d == 6)
	    break;
	data[d] = (p ? data[d] << 4 : 0) + digit;
	++p;
    }

    if (s == end && p != 0 && d == 5) {
	memcpy(&value, data, 6);
	return true;
    }

#if !CLICK_TOOL
    return AddressInfo::query_ethernet(str, value.data(), args.context(), flags);
#else
    (void) args, (void) flags;
    return false;
#endif
}

bool
EtherAddressArg::direct_parse(const String& str, EtherAddress& value, Args& args,
                              int flags)
{
    EtherAddress *s = args.slot(value);
    return s && parse(str, *s, args, flags);
}

CLICK_ENDDECLS
