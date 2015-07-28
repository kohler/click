// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * settimestampdelta.{cc,hh} -- element observes range of timestamps
 * Eddie Kohler
 *
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/error.hh>
#include "settimestampdelta.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

SetTimestampDelta::SetTimestampDelta()
{
}

SetTimestampDelta::~SetTimestampDelta()
{
}

int
SetTimestampDelta::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String typ = "RANGE";
    if (Args(conf, this, errh)
	.read("TYPE", WordArg(), typ)
	.complete() < 0)
	return -1;
    if (typ == "RANGE")
	_type = 0;
    else if (typ == "NOW")
	_type = 1;
    else if (typ == "FIRST")
	_type = 2;
    else
	return errh->error("bad TYPE");
    return 0;
}

Packet *
SetTimestampDelta::simple_action(Packet *p)
{
    Timestamp& tv = p->timestamp_anno();
    if (tv)
	switch (_type) {
	case 0:
	    if (!_first)
		_first = tv;
	    tv -= _first;
	    break;
	case 1:
	    tv = Timestamp::now() - tv;
	    break;
	case 2:
	    tv -= FIRST_TIMESTAMP_ANNO(p);
	    break;
	}
    return p;
}

String
SetTimestampDelta::read_handler(Element *e, void *thunk)
{
    SetTimestampDelta *td = static_cast<SetTimestampDelta *>(e);
    StringAccum sa;
    switch ((intptr_t)thunk) {
      case 0: {
	  StringAccum sa;
	  sa << td->_first << '\n';
	  return sa.take_string();
      }
      default:
	return "<error>\n";
    }
}

int
SetTimestampDelta::write_handler(const String &, Element *e, void *, ErrorHandler *)
{
    SetTimestampDelta *td = static_cast<SetTimestampDelta *>(e);
    td->_first = Timestamp();
    return 0;
}

void
SetTimestampDelta::add_handlers()
{
    add_read_handler("first", read_handler);
    add_write_handler("reset", write_handler, 0, Handler::f_button);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTimestampDelta)
