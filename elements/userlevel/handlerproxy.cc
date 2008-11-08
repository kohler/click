/*
 * handlerproxy.{cc,hh} -- handler proxy superclass
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include "handlerproxy.hh"
#include <click/error.hh>
CLICK_DECLS

HandlerProxy::HandlerProxy()
    : _err_rcvs(0), _nerr_rcvs(0)
{
}

HandlerProxy::~HandlerProxy()
{
}

int
HandlerProxy::add_error_receiver(ErrorReceiverHook hook, void *thunk)
{
    ErrorReceiver *new_err_rcvs = new ErrorReceiver[_nerr_rcvs + 1];
    if (!new_err_rcvs) return -ENOMEM;
    memcpy(new_err_rcvs, _err_rcvs, sizeof(ErrorReceiver) * _nerr_rcvs);
    new_err_rcvs[_nerr_rcvs].hook = hook;
    new_err_rcvs[_nerr_rcvs].thunk = thunk;
    delete[] _err_rcvs;
    _err_rcvs = new_err_rcvs;
    _nerr_rcvs++;
    return 0;
}

int
HandlerProxy::remove_error_receiver(ErrorReceiverHook hook, void *thunk)
{
    for (int i = 0; i < _nerr_rcvs; i++)
	if (_err_rcvs[i].hook == hook && _err_rcvs[i].thunk == thunk) {
	    memcpy(&_err_rcvs[i], &_err_rcvs[_nerr_rcvs - 1], sizeof(ErrorReceiver));
	    _nerr_rcvs--;
	    return 0;
	}
    return -1;
}

int
HandlerProxy::check_handler(const String &hname, bool, ErrorHandler *errh)
{
    errh->error("{ec:%d}Handler '%#s' status unknown", CSERR_UNSPECIFIED, hname.printable().c_str());
    return -1;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
ELEMENT_PROVIDES(HandlerProxy)
