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
	if (cp_va_space_kparse(conf[i], this, errh,
			       "ACTION", cpkP+cpkM, cpWord, &action,
			       "TYPE", cpkP+cpkM, cpWord, &type,
			       "ARG", cpkP+cpkM, cpArgument, &arg,
			       cpEnd) < 0)
	    return -1;
	if (action != "drop" || type != "dev" || !arg)
	    return errh->error("arguments must follow 'drop dev DEVNAME'");
	_drop_devices.push_back(arg);
    }
    return 0;
}

int
KernelFilter::initialize(ErrorHandler *errh)
{
    // If you update this, also update the device_filter code in FromDevice.u
    int before = errh->nerrors();
    for (int i = 0; i < _drop_devices.size(); ++i)
	if (device_filter(_drop_devices[i], true, errh) < 0)
	    _drop_devices[i] = String();
    return before == errh->nerrors() ? 0 : -1;
}

void
KernelFilter::cleanup(CleanupStage stage)
{
    if (stage >= CLEANUP_INITIALIZED) {
	ErrorHandler *errh = ErrorHandler::default_handler();
	for (int i = _drop_devices.size() - 1; i >= 0; --i)
	    if (_drop_devices[i])
		device_filter(_drop_devices[i], false, errh);
    }
}

int
KernelFilter::device_filter(const String &devname, bool add_filter,
			    ErrorHandler *errh)
{
    StringAccum cmda;
    cmda << "/sbin/iptables " << (add_filter ? "-A" : "-D") << " INPUT -i "
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
