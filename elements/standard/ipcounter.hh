/*
* ipcounter.hh -- Counter Element utilizing HashTable_RCU
* Neil McGlohon

* Copyright (c) 2017 Cisco Meraki
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
*
*/


// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_COUNTER_HH
#define CLICK_COUNTER_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <click/vector.hh>
#include <click/llrpc.h>
#include <click/hashtable_rcu.hh>

CLICK_CXX_PROTECT
#if CLICK_LINUXMODULE
#include <click/cxxprotect.h>
#include <linux/rculist.h>
#include <click/cxxunprotect.h>
#endif
CLICK_CXX_UNPROTECT

CLICK_DECLS
class HandlerCall;


class IPCounter : public Element {
public:
	IPCounter() CLICK_COLD;
	~IPCounter() CLICK_COLD;

	const char *class_name() const { return "IPCounter"; }
	const char *port_count() const { return PORTS_1_1;   }

	void reset();

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	int initialize(ErrorHandler *) CLICK_COLD;
	void add_handlers() CLICK_COLD;

	Packet *simple_action(Packet *);

protected:

#ifdef HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif

	counter_t count;
	HashTable_RCU<String,int>* IPPacketTable_RCU;

	static String read_handler(Element *, void *) CLICK_COLD;
	static int write_handler(const String &arg, Element *e, void *thunk, ErrorHandler *errh) CLICK_COLD;
};

CLICK_ENDDECLS
#endif
