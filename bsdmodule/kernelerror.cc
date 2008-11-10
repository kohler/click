/*
 * kernelerror.{cc,hh} -- ErrorHandler subclass that saves errors for
 * /proc/click/errors(XXX) and reports them with printk()
 * Eddie Kohler, Nickolai Zeldovich
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "modulepriv.hh"

#include "kernelerror.hh"
#include <click/straccum.hh>
CLICK_USING_DECLS

static StringAccum *all_errors = 0;

CLICK_DECLS

void *
KernelErrorHandler::emit(const String &str, void *, bool)
{
    String landmark;
    const char *s = parse_anno(str, str.begin(), str.end(),
			       "l", &landmark, (const char *) 0);
    landmark = clean_landmark(landmark, true);
    printf("%.*s%.*s\n", landmark.length(), landmark.begin(), str.end() - s, s);
    if (all_errors)
	*all_errors << landmark << str.substring(s, str.end()) << '\n';
    return 0;
}

void
KernelErrorHandler::account(int level)
{
    ErrorHandler::account(level);
    if (level <= err_fatal)
	panic("click");
}

CLICK_ENDDECLS
