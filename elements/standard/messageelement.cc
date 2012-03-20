// -*- c-basic-offset: 4 -*-
/*
 * messageelement.{cc,hh} -- prints a message on configuration
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "messageelement.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

MessageElement::MessageElement()
{
}

int
MessageElement::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String message, type = "MESSAGE";
    if (Args(conf, this, errh)
	.read_mp("MESSAGE", message)
	.read_p("TYPE", KeywordArg(), type).complete() < 0)
	return -1;
    const char *err;
    if (type == "MESSAGE")
	err = ErrorHandler::e_info;
    else if (type == "WARNING")
	err = ErrorHandler::e_warning_annotated;
    else if (type == "ERROR")
	err = ErrorHandler::e_error;
    else
	return errh->error("unrecognized message type");
    return errh->xmessage(err, message);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MessageElement)
