// -*- related-file-name: "../include/click/etheraddress.hh" -*-
/*
 * etheraddress.{cc,hh} -- an Ethernet address class. Useful for its
 * hashcode() method
 * Robert Morris / John Jannotti, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2006 Regents of the University of California
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
CLICK_DECLS

/** @file etheraddress.hh
 * @brief The EtherAddress type for Ethernet addresses.
 */

/** @class EtherAddress
    @brief An Ethernet address.

    The EtherAddress type represents an Ethernet address. It supports equality
    operations and provides methods for unparsing addresses into ASCII form. */

/** @brief Unparses this address into a dash-separated hex String.

    Examples include "00-00-00-00-00-00" and "00-05-4E-50-3C-1A".

    @note The IEEE standard for printing Ethernet addresses uses dashes as
    separators, not colons.  Use unparse_colon() to unparse into the
    nonstandard colon-separated form. */
String
EtherAddress::unparse() const
{
    String str = String::garbage_string(17);
    // NB: mutable_c_str() creates space for the terminating null character
    if (char *x = str.mutable_c_str()) {
	const unsigned char *p = this->data();
	sprintf(x, "%02X-%02X-%02X-%02X-%02X-%02X",
		p[0], p[1], p[2], p[3], p[4], p[5]);
    }
    return str;
}

/** @brief Unparses this address into a colon-separated hex String.

    Examples include "00:00:00:00:00:00" and "00:05:4E:50:3C:1A".

    @note Use unparse() to create the IEEE standard dash-separated form. */
String
EtherAddress::unparse_colon() const
{
    String str = String::garbage_string(17);
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

CLICK_ENDDECLS
