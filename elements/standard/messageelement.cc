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
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

MessageElement::MessageElement()
{
    MOD_INC_USE_COUNT;
}

MessageElement::~MessageElement()
{
    MOD_DEC_USE_COUNT;
}

int
MessageElement::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String message, warning, error;
    if (cp_va_parse(conf, this, errh,
		    cpOptional,
		    cpString, "message", &message,
		    cpKeywords,
		    "MESSAGE", cpString, "message", &message,
		    "WARNING", cpString, "warning message", &warning,
		    "ERROR", cpString, "error message", &error,
		    0) < 0)
	return -1;
    if (message)
	errh->verror_text(ErrorHandler::ERR_MESSAGE, String(), message);
    if (warning)
	errh->verror_text(ErrorHandler::ERR_WARNING, String(), warning);
    if (error)
	errh->verror_text(ErrorHandler::ERR_ERROR, String(), error);
    return (error ? -1 : 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MessageElement)
