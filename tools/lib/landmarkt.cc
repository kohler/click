// -*- c-basic-offset: 4 -*-
/*
 * landmarkt.{cc,hh} -- sets of landmarks
 * Eddie Kohler
 *
 * Copyright (c) 2007 Regents of the University of California
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
#include <click/straccum.hh>
#include <click/error.hh>
#include <algorithm>

#include "landmarkt.hh"

LandmarkT *LandmarkT::empty;

void
LandmarkT::static_initialize()
{
    if (!empty)
	empty = new LandmarkT(new LandmarkSetT, noffset, noffset);
}

LandmarkSetT::LandmarkSetT()
    : _refcount(1)
{
}

LandmarkSetT::LandmarkSetT(const String &filename, unsigned lineno)
    : _refcount(1)
{
    _linfo.push_back(LandmarkInfo(LandmarkT::noffset - 1, 0, lineno));
    _fnames.push_back(filename);
}

LandmarkSetT::~LandmarkSetT()
{
    assert(_refcount == 0);
}

void
LandmarkSetT::new_line(unsigned offset, const String &filename, unsigned lineno)
{
    if (_fnames.size() == 0 || _fnames.back() != filename)
	_fnames.push_back(filename);
    if (_linfo.size())
	_linfo.back().end_offset = offset;
    _linfo.push_back(LandmarkInfo(LandmarkT::noffset - 1, _fnames.size() - 1, lineno));
}

static bool li_searcher(const LandmarkSetT::LandmarkInfo &li, unsigned offset)
{
    return li.end_offset <= offset;
}

String
LandmarkSetT::offset_to_string(unsigned offset) const
{
    Vector<LandmarkInfo>::const_iterator i =
	std::lower_bound(_linfo.begin(), _linfo.end(), offset, li_searcher);
    if (i == _linfo.end())
	i = _linfo.begin();
    if (i != _linfo.end()) {
	const String &fname = _fnames[i->filename];
	if (fname && i->lineno)
	    return fname + ":" + String(i->lineno);
	else if (fname)
	    return fname;
	else if (i->lineno)
	    return String::make_stable("line ", 5) + String(i->lineno);
    }
    return String::make_stable("<unknown>", 9);
}

String
LandmarkSetT::offset_to_decorated_string(unsigned offset1, unsigned offset2) const
{
    Vector<LandmarkInfo>::const_iterator i =
	std::lower_bound(_linfo.begin(), _linfo.end(), offset1, li_searcher);
    if (i == _linfo.end())
	i = _linfo.begin();
    StringAccum sa;
    if (i != _linfo.end()) {
	const String &fname = _fnames[i->filename];
	if (fname && i->lineno)
	    sa << fname << ':' << i->lineno;
	else if (fname)
	    sa << fname;
	else if (i->lineno)
	    sa << "line " << i->lineno;
    }
    if (!sa)
	sa << "<unknown>";
    if (offset1 != LandmarkT::noffset) {
	String x = ErrorHandler::make_landmark_anno(sa.take_string());
	sa.clear();
	sa << x << "{l1:" << offset1 << "}{l2:" << offset2 << '}';
	return sa.take_string();
    } else
	return sa.take_string();
}
