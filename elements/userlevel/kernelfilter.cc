// -*- c-basic-offset: 4 -*-
/*
 * kernelfilter.{cc,hh} -- element runs iptables to block kernel processing
 * Eddie Kohler
 *
 * Copyright (c) 2007 Regents of the University of California
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
#include "kernelfilter.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
CLICK_DECLS

KernelFilter::KernelFilter()
{
}

KernelFilter::~KernelFilter()
{
}

int
KernelFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String action, type, arg;
    for (int i = 0; i < conf.size(); i++) {
	if (cp_va_space_parse(conf[i], this, errh,
			      cpWord, "action", &action,
			      cpWord, "type", &type,
			      cpArgument, "arg", &arg,
			      cpEnd) < 0)
	    return -1;
	if (action != "drop" || type != "dev" || !arg)
	    return errh->error("arguments must follow 'drop dev DEVNAME'");
	_filters.push_back("INPUT -i " + shell_quote(arg) + " -j DROP");
    }
    return 0;
}

int
KernelFilter::initialize(ErrorHandler *errh)
{
    int before = errh->nerrors();
    for (int i = 0; i < _filters.size(); i++) {
	String out = shell_command_output_string("/sbin/iptables -A " + _filters[i], "", errh);
	if (errh->nerrors() != before)
	    _filters.resize(i);
	else if (out) {
	    errh->error("iptables -A %s: %s", _filters[i].c_str(), out.c_str());
	    _filters.resize(i);
	}
    }
    return before == errh->nerrors() ? 0 : -1;
}

void
KernelFilter::cleanup(CleanupStage stage)
{
    if (stage >= CLEANUP_INITIALIZED) {
	ErrorHandler *errh = ErrorHandler::default_handler();
	for (int i = _filters.size() - 1; i >= 0; i++) {
	    String out = shell_command_output_string("/sbin/iptables -D " + _filters[i], "", errh);
	    if (out)
		errh->error("iptables -D %s: %s", _filters[i].c_str(), out.c_str());
	}
    }
}

int
KernelFilter::device_filter(const String &devname, bool add, ErrorHandler *errh)
{
    StringAccum cmda;
    cmda << "/sbin/iptables " << (add ? "-A" : "-D") << " INPUT -i "
	 << shell_quote(devname) << " -j DROP";
    String cmd = cmda.take_string();
    int before = errh->nerrors();
    String out = shell_command_output_string(cmd, "", errh);
    if (out)
	errh->error("%s: %s", cmd.c_str(), out.c_str());
    return errh->nerrors() == before ? 0 : -1;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(KernelFilter)
