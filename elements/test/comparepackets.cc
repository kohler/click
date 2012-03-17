// -*- c-basic-offset: 4 -*-
/*
 * comparepackets.{cc,hh} -- test element that checks packet contents
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
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
#include "comparepackets.hh"
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/args.hh>
CLICK_DECLS

ComparePackets::ComparePackets()
    : _ndiff(0)
{
    _p[0] = _p[1] = 0;
    _available[0] = _available[1] = true;
    memset(_diff_details, 0, sizeof(_diff_details));
}

ComparePackets::~ComparePackets()
{
}

int
ComparePackets::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool timestamp = true;
    if (Args(conf, this, errh).read("TIMESTAMP", timestamp).complete() < 0)
	return -1;
    _timestamp = timestamp;
    return 0;
}

int
ComparePackets::initialize(ErrorHandler *)
{
    _signal[0] = Notifier::upstream_empty_signal(this, 0);
    _signal[1] = Notifier::upstream_empty_signal(this, 1);
    return 0;
}

void
ComparePackets::cleanup(CleanupStage)
{
    if (_p[0])
	_p[0]->kill();
    if (_p[1])
	_p[1]->kill();
}

void
ComparePackets::check(Packet *p, Packet *q)
{
    bool different = false;

    if (p->length() != q->length())
	_diff_details[D_LEN]++, different = true;
    if (memcmp(p->data(), q->data(), p->length()) != 0)
	_diff_details[D_DATA]++, different = true;
    if (p->timestamp_anno() != q->timestamp_anno() && _timestamp)
	_diff_details[D_TIMESTAMP]++, different = true;

    if (p->has_network_header() && q->has_network_header()) {
	if (p->network_header_offset() != q->network_header_offset())
	    _diff_details[D_NETOFF]++, different = true;
	if (p->network_header_length() != q->network_header_length())
	    _diff_details[D_NETLEN]++, different = true;
    } else if (p->has_network_header() != q->has_network_header())
	_diff_details[D_NETHDR]++, different = true;

    if (different)
	_ndiff++;
}

Packet *
ComparePackets::pull(int port)
{
    assert(_available[0] || _available[1]);
    Packet *retval = 0;
    if (_p[port]) {
	if (_available[port])
	    return 0;
	else {
	    retval = _p[port];
	    _available[port] = true;
	}
    }
    _p[port] = input(port).pull();
    if (!_p[port]) {
	// different numbers of packets upstream
	if (!_signal[port] && _p[!port] && _available[!port]) {
	    _diff_details[D_MORE_PACKETS_0 + !port]++, _ndiff++;
	    _available[!port] = false;
	}
    } else if (_p[!port] && _available[!port]) {
	check(_p[0], _p[1]);
	if (retval)
	    _available[port] = false;
	else {
	    retval = _p[port];
	    _p[port] = 0;
	}
	_available[!port] = false;
    }
    return retval;
}

enum { H_DIFFS, H_DIFF_DETAILS, H_ALL_SAME };

static const char * const reason_texts[] = {
    "different length", "different data", "different timestamp",
    "different network header offset", "different network header length",
    "different network header presence",
    "more packets in [0]", "more packets in [1]"
};

String
ComparePackets::read_handler(Element *e, void *thunk)
{
    ComparePackets *cp = static_cast<ComparePackets *>(e);
    switch ((uintptr_t) thunk) {
      case H_DIFFS:
	return String(cp->_ndiff);
      case H_DIFF_DETAILS: {
	  StringAccum sa;
	  for (int i = 0; i < D_LAST; i++)
	      sa << cp->_diff_details[i] << '\t' << reason_texts[i] << '\n';
	  return sa.take_string();
      }
      case H_ALL_SAME:
	return BoolArg::unparse(cp->_ndiff == 0);
      default:
	return "<error>";
    }
}

void
ComparePackets::add_handlers()
{
    add_read_handler("diffs", read_handler, H_DIFFS);
    add_read_handler("diff_details", read_handler, H_DIFF_DETAILS);
    add_read_handler("all_same", read_handler, H_ALL_SAME);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ComparePackets)
