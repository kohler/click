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
#include <click/etheraddress.hh>
CLICK_DECLS

namespace {
class ErrorTestHandler : public BaseErrorHandler { public:
    ErrorTestHandler() { }
    void handle_text(Seriousness s, const String &text) {
	_seriousness = s;
	_text = text;
    }
    bool check(Seriousness s, const String &text) {
	return (_seriousness == s && _text == text);
    }
    Seriousness _seriousness;
    String _text;
};
}

ErrorTest::ErrorTest()
{
}

ErrorTest::~ErrorTest()
{
}

#define CHECK(s, text) if (!errh.check((s), (text))) return init_errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, (text));

int
ErrorTest::initialize(ErrorHandler *init_errh)
{
    ErrorTestHandler errh;

    IPAddress ipa(String("1.0.2.3"));
    EtherAddress etha;
    cp_ethernet_address("0:1:3:5:A:B", &etha);
    errh.error("IP %{ip_ptr} %% ETH %{ether_ptr}", &ipa, &etha);
    CHECK(ErrorHandler::ERR_ERROR, "IP 1.0.2.3 % ETH 00-01-03-05-0A-0B");

    init_errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(ErrorTest)
CLICK_ENDDECLS
