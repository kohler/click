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
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <unistd.h>
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
    if (Args(this, errh).bind(conf)
	.read("IPTABLES_COMMAND", _iptables_command)
	.consume() < 0)
	return -1;
    String action, type, arg;
    for (int i = 0; i < conf.size(); i++) {
	if (Args(this, errh).push_back_words(conf[i])
	    .read_mp("ACTION", WordArg(), action)
	    .read_mp("TYPE", WordArg(), type)
	    .read_mp("ARG", AnyArg(), arg)
	    .complete() < 0)
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
    for (int i = 0; i < _drop_devices.size(); ++i)
	if (device_filter(_drop_devices[i], true, errh, _iptables_command) < 0)
	    _drop_devices[i] = String();
    return errh->nerrors() ? -1 : 0;
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
			    ErrorHandler *errh,
			    const String &iptables_command)
{
    StringAccum cmda;
    if (iptables_command)
	cmda << iptables_command;
    else if (access("/sbin/iptables", X_OK) == 0)
	cmda << "/sbin/iptables";
    else if (access("/usr/sbin/iptables", X_OK) == 0)
	cmda << "/usr/sbin/iptables";
    else
	return errh->error("no %<iptables%> executable found");
    cmda << " " << (add_filter ? "-A" : "-D") << " INPUT -i "
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
