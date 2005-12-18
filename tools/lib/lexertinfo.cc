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
LexerTInfo::notify_comment(const char *, const char *)
{
}

void
LexerTInfo::notify_error(const String &, const char *, const char *)
{
}

void
LexerTInfo::notify_line_directive(const char *, const char *)
{
}

void
LexerTInfo::notify_keyword(const String &, const char *, const char *)
{
}

void
LexerTInfo::notify_config_string(const char *, const char *)
{
}

void
LexerTInfo::notify_class_declaration(ElementClassT *, bool, const char *, const char *, const char *)
{
}

void
LexerTInfo::notify_class_extension(ElementClassT *, const char *, const char *)
{
}

void
LexerTInfo::notify_class_reference(ElementClassT *, const char *, const char *)
{
}

void
LexerTInfo::notify_element_declaration(ElementT *, const char *, const char *, const char *)
{
}

void
LexerTInfo::notify_element_reference(ElementT *, const char *, const char *)
{
}
