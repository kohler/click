// -*- c-basic-offset: 4 -*-
/*
 * lexertinfo.{cc,hh} -- notified by LexerT about parsing events
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

#include "lexertinfo.hh"

void
LexerTInfo::notify_comment(int, int)
{
}

void
LexerTInfo::notify_error(const String &, int, int)
{
}

void
LexerTInfo::notify_line_directive(int, int)
{
}

void
LexerTInfo::notify_keyword(const String &, int, int)
{
}

void
LexerTInfo::notify_config_string(int, int)
{
}

void
LexerTInfo::notify_class_declaration(ElementClassT *, bool, int, int, int)
{
}

void
LexerTInfo::notify_class_extension(ElementClassT *, int, int)
{
}

void
LexerTInfo::notify_class_reference(ElementClassT *, int, int)
{
}

void
LexerTInfo::notify_element_declaration(ElementT *, int, int, int)
{
}

void
LexerTInfo::notify_element_reference(ElementT *, int, int)
{
}
