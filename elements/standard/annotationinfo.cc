// -*- c-basic-offset: 4 -*-
/*
 * annotationinfo.{cc,hh} -- element stores packet annotation information
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include "annotationinfo.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
CLICK_DECLS

AnnotationInfo::AnnotationInfo()
{
}

int
AnnotationInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    for (int i = 0; i < conf.size(); i++) {
	String str = conf[i];
	String name_str = cp_shift_spacevec(str);
	if (!name_str		// allow empty arguments
	    || name_str[0] == '#' // allow comments
	    || name_str.equals("CHECK_OVERLAP", 13)) // check in initialize()
	    continue;

	String offset_str = cp_shift_spacevec(str);
	String size_str = cp_shift_spacevec(str);

	int offset, size = 0;
	if (!cp_is_word(name_str) || cp_anno(name_str, 0, &offset, 0))
	    errh->error("bad NAME %<%s%>", name_str.c_str());
	else if (!cp_anno(offset_str, 0, &offset, this)
		 || (size_str && !IntArg().parse(size_str, size))
		 || size < 0
		 || ANNOTATIONINFO_OFFSET(offset) + size > Packet::anno_size
		 || (size && ANNOTATIONINFO_SIZE(offset)
		     && size != ANNOTATIONINFO_SIZE(offset))
		 || str)
	    errh->error("bad entry for %<%s%>", name_str.c_str());
	else {
	    uint32_t anno = MAKE_ANNOTATIONINFO(ANNOTATIONINFO_OFFSET(offset), size);
	    NameInfo::define(NameInfo::T_ANNOTATION, this, name_str, &anno, 4);
	}
    }

    return errh->nerrors() ? -1 : 0;
}

int
AnnotationInfo::initialize(ErrorHandler *errh)
{
    Vector<String> conf;
    cp_argvec(configuration(), conf);
    for (int i = 0; i < conf.size(); i++) {
	Vector<String> words;
	cp_spacevec(conf[i], words);
	if (words.size() == 0 || !words[0].equals("CHECK_OVERLAP", 13))
	    continue;

	int offset;
	Vector<int> offsets(words.size(), -1);
	for (int i = 1; i < words.size(); ++i)
	    if (cp_anno(words[i], 0, &offset, this))
		offsets[i] = offset;
	    else
		errh->error("bad ANNO %<%s%>", words[i].c_str());

	for (int i = 1; i < words.size(); ++i)
	    for (int j = i + 1; j < words.size(); ++j) {
		int isize = ANNOTATIONINFO_SIZE(offsets[i]);
		int jsize = ANNOTATIONINFO_SIZE(offsets[j]);
		isize = (isize <= 0 ? 1 : isize);
		jsize = (jsize <= 0 ? 1 : jsize);
		if (offsets[i] == -1
		    || offsets[j] == -1
		    || (ANNOTATIONINFO_OFFSET(offsets[i]) + isize <= ANNOTATIONINFO_OFFSET(offsets[j]))
		    || (ANNOTATIONINFO_OFFSET(offsets[j]) + jsize <= ANNOTATIONINFO_OFFSET(offsets[i])))
		    /* OK */;
		else
		    errh->error("annotations %<%s%> and %<%s%> conflict", words[i].c_str(), words[j].c_str());
	    }
    }

    return errh->nerrors() ? -1 : 0;
}

EXPORT_ELEMENT(AnnotationInfo)
CLICK_ENDDECLS
