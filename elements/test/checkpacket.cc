// -*- c-basic-offset: 4 -*-
/*
 * checkpacket.{cc,hh} -- test element that checks packet contents
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
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
#include "checkpacket.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

CheckPacket::CheckPacket()
{
}

int
CheckPacket::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _data = String::make_out_of_memory();
    _data_offset = 0;
    String alignment;
    int length_eq = -1, length_ge = -1, length_le = -1;

    if (Args(conf, this, errh)
	.read("DATA", _data)
	.read("DATA_OFFSET", _data_offset)
	.read("LENGTH", length_eq)
	.read("LENGTH_EQ", length_eq)
	.read("LENGTH_GE", length_ge)
	.read("LENGTH_LE", length_le)
	.read("ALIGNMENT", AnyArg(), alignment)
	.complete() < 0)
	return -1;

    if ((length_eq >= 0) + (length_ge >= 0) + (length_le >= 0) > 1)
	return errh->error("specify at most one of LENGTH_EQ, LENGTH_GE, and LENGTH_LE");
    else if (length_eq >= 0)
	_length = length_eq, _length_op = '=';
    else if (length_ge >= 0)
	_length = length_ge - 1, _length_op = '>';
    else if (length_le >= 0)
	_length = length_le + 1, _length_op = '<';
    else
	_length_op = 0;

    _data_op = (_data.out_of_memory() ? 0 : '=');

    if (alignment) {
	if (Args(this, errh).push_back_words(alignment)
	    .read_mp("MODULUS", _alignment_chunk)
	    .read_mp("OFFSET", _alignment_offset)
	    .complete() < 0)
	    return -1;
	else if (_alignment_chunk <= 1 || _alignment_offset < 0 || _alignment_offset >= _alignment_chunk)
	    return errh->error("bad alignment modulus and/or offset");
    }

    return 0;
}

Packet *
CheckPacket::simple_action(Packet *p)
{
    ErrorHandler *errh = ErrorHandler::default_handler();

    // check length
    if (_length_op == '=') {
	if (p->length() != _length)
	    errh->error("%s: bad length %d (wanted %d)", declaration().c_str(), p->length(), _length);
    } else if (_length_op == '>') {
	if (p->length() <= _length)
	    errh->error("%s: bad length %d (wanted > %d)", declaration().c_str(), p->length(), _length);
    } else if (_length_op == '<') {
	if (p->length() >= _length)
	    errh->error("%s: bad length %d (wanted < %d)", declaration().c_str(), p->length(), _length);
    }

    // check data
    if (_data_op) {
	if (p->length() < _data.length() + _data_offset)
	    errh->error("%s: data too short (%d bytes, wanted %d)", declaration().c_str(), p->length(), _data.length() + _data_offset);
	else if (_data_op == '=' && p->length() > _data.length() + _data_offset)
	    errh->error("%s: data too long (%d bytes, wanted %d)", declaration().c_str(), p->length(), _data.length() + _data_offset);
	else if (memcmp(p->data() + _data_offset, _data.data(), _data.length()) != 0)
	    errh->error("%s: bad data (does not match)", declaration().c_str());
    }

    // check alignment
    if (_alignment_chunk >= 0) {
	int alignment = reinterpret_cast<uintptr_t>(p->data()) & (_alignment_chunk - 1);
	if (alignment != _alignment_offset)
	    errh->error("%s: bad alignment (%d/%d, expected %d/%d)", declaration().c_str(), _alignment_chunk, alignment, _alignment_chunk, _alignment_offset);
    }

    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckPacket)
