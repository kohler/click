// -*- c-basic-offset: 4 -*-
/*
 * errortest.{cc,hh} -- regression test element for configuration parsing
 * Eddie Kohler
 *
 * Copyright (c) 2008 Regents of the University of California
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
#include "errortest.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

namespace {
class ErrorTestHandler : public ErrorHandler { public:
    ErrorTestHandler() { }
    void *emit(const String &str, void *, bool) {
	_text << str << '\n';
	return 0;
    }
    bool check(const String &text) {
	_last_text = _text.take_string();
	return text == _last_text;
    }
    StringAccum _text;
    String _last_text;
};
}

ErrorTest::ErrorTest()
{
}

#define CHECK(text) if (!errh.check((text))) return init_errh->error("%s:%d: test %<%s%> failed, got %<%.*s%>", __FILE__, __LINE__, (text), errh._last_text.length(), errh._last_text.data());

int
ErrorTest::initialize(ErrorHandler *init_errh)
{
    ErrorTestHandler errh;

    IPAddress ipa(String("1.0.2.3"));
    EtherAddress etha;
    EtherAddressArg().parse("0:1:3:5:A:B", etha);
    errh.error("IP %p{ip_ptr} %% ETH %p{ether_ptr}", &ipa, &etha);
    CHECK("<3>IP 1.0.2.3 % ETH 00-01-03-05-0A-0B\n");

    {
	ContextErrorHandler cerrh(&errh, "Context:");
	cerrh.error("Testing context 1");
	CHECK("<3>{context:context}Context:\n<3>  Testing context 1\n");
	cerrh.error("Testing context 2");
	CHECK("<3>  Testing context 2\n");
    }

    {
	ContextErrorHandler cerrh(&errh, "Context:");
	PrefixErrorHandler perrh(&errh, "{context:no}");
	perrh.error("Testing context 1");
	CHECK("{context:no}<3>Testing context 1\n");
	perrh.error("Testing context 2");
	CHECK("{context:no}<3>Testing context 2\n");
    }

    {
	ContextErrorHandler cerrh(&errh, "Context:");
	cerrh.error("{context:no}Testing context 1");
	CHECK("<3>{context:no}Testing context 1\n");
	cerrh.error("Testing context 2");
	CHECK("<3>{context:context}Context:\n<3>  Testing context 2\n");
    }

    {
	ContextErrorHandler cerrh(&errh, "Context:");
	cerrh.error("{context:nocontext}Testing context 1");
	CHECK("<3>{context:nocontext}  Testing context 1\n");
	cerrh.error("{context:noindent}Testing context 2");
	CHECK("<3>{context:context}Context:\n<3>{context:noindent}Testing context 2\n");
    }

    {
	ContextErrorHandler cerrh(&errh, "Context %<foo%>:");
	cerrh.error("Testing context 1");
	CHECK("<3>{context:context}Context 'foo':\n<3>  Testing context 1\n");
	cerrh.error("Testing context 2");
	CHECK("<3>  Testing context 2\n");
    }

    {
	char *x = new char[4];
	memcpy(x, "Hi!!", 4);
	errh.error("This should not cause memory errors: %<%.*s%>", 4, x);
	CHECK("<3>This should not cause memory errors: 'Hi!!'\n");
    }

    init_errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(ErrorTest)
CLICK_ENDDECLS
