// -*- related-file-name: "../include/click/archive.hh" -*-
/*
 * archive.{cc,hh} -- deal with `ar' files
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

#include <click/glue.hh>
#include <click/archive.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

/* `ar' file format:

   "!<arch>\n"
8    first header block
68   first file data
68+n second header block
     etc.

   header block format:

 0    char ar_name[16];		// Member file name, sometimes / terminated.
16    char ar_date[12];		// File date, decimal seconds since Epoch.
28    char ar_uid[6],
34         ar_gid[6];		// User and group IDs, in ASCII decimal.
40    char ar_mode[8];		// File mode, in ASCII octal.
48    char ar_size[10];		// File size, in ASCII decimal.
58    char ar_fmag[2];		// Always contains ARFMAG == "`\n".
*/

CLICK_DECLS

static inline int
read_uint(const char *data, int max_len,
	  const char *type, ErrorHandler *errh, int base = 10)
{
    int x;
    const char *end = cp_integer(data, data + max_len, base, &x);
    if (end == data || (end < data + max_len && !isspace((unsigned char) *end))) {
	x = -1;
	errh->warning("bad %s in archive", type);
    }
    return x;
}

int
ArchiveElement::parse(const String &str, Vector<ArchiveElement> &ar,
		      ErrorHandler *errh)
{
    LocalErrorHandler lerrh(errh);
    ar.clear();

    if (str.length() <= 8 || memcmp(str.data(), "!<arch>\n", 8) != 0)
	return lerrh.error("not an archive");

    ArchiveElement longname_ae;

    const char *s = str.begin() + 8, *begin_data;

    // loop over sections
    while ((begin_data = s + 60) <= str.end()) {
	// check magic number
	if (s[58] != '`' || s[59] != '\n')
	    return lerrh.error("bad archive: missing header magic number");

	int size;

	if (s[0] == '/' && s[1] == '/' && isspace((unsigned char) s[2])) {
	    // GNUlike long name section
	    if (longname_ae.data)
		lerrh.error("two long name sections in archive");
	    size = read_uint(s + 48, 10, "size", &lerrh);
	    if (size < 0 || begin_data + size > str.end())
		return lerrh.error("truncated archive");
	    longname_ae.data = str.substring(begin_data, begin_data + size);

	} else {
	    ArchiveElement ae;

	    // read name
	    int bsd_longname = 0;
	    if (s[0] == '/' && s[1] >= '0' && s[1] <= '9') {
		int offset = read_uint(s + 1, 15, "long name", &lerrh);
		if (!longname_ae.data || offset < 0
		    || offset >= longname_ae.data.length())
		    lerrh.error("bad long name in archive");
		else {
		    const char *ndata = longname_ae.data.begin() + offset;
		    const char *nend = ndata;
		    while (nend < longname_ae.data.end() && *nend != '/'
			   && !isspace((unsigned char) *nend))
			++nend;
		    ae.name = longname_ae.data.substring(ndata, nend);
		}
	    } else if (s[0] == '#' && s[1] == '1' && s[2] == '/'
		       && s[3] >= '0' && s[3] <= '9') {
		bsd_longname = read_uint(s + 3, 13, "long name", &lerrh);
	    } else {
		const char *x = s;
		while (x < s + 16 && *x != '/' && !isspace((unsigned char) *x))
		    ++x;
		ae.name = str.substring(s, x);
	    }

	    // read date, uid, gid, mode, size
	    ae.date = read_uint(s + 16, 12, "date", &lerrh);
	    ae.uid = read_uint(s + 28, 6, "uid", &lerrh);
	    ae.gid = read_uint(s + 34, 6, "gid", &lerrh);
	    ae.mode = read_uint(s + 40, 8, "mode", &lerrh, 8);
	    size = read_uint(s + 48, 10, "size", &lerrh);
	    const char *end_data = begin_data + size;
	    if (size < 0 || end_data > str.end())
		return lerrh.error("truncated archive, %d bytes short", (int) (end_data - str.end()));

	    // set data
	    if (bsd_longname > 0) {
		if (size < bsd_longname)
		    return lerrh.error("bad long name in archive");
		ae.name = str.substring(begin_data, begin_data + bsd_longname);
		ae.data = str.substring(begin_data + bsd_longname, end_data);
	    } else
		ae.data = str.substring(begin_data, end_data);

	    // append archive element
	    ar.push_back(ae);
	}

	s = begin_data + size;
	if (size % 2 == 1)	// objects in archive are even # of bytes
	    ++s;
    }

    if (s != str.end())
	return lerrh.error("truncated archive");
    else
	return 0;
}

String
ArchiveElement::unparse(const Vector<ArchiveElement> &ar, ErrorHandler *errh)
{
    LocalErrorHandler lerrh(errh);

    StringAccum sa;
    int want_size = 8;
    sa << "!<arch>\n";
    for (const ArchiveElement *ae = ar.begin(); ae != ar.end(); ++ae) {
	if (!ae->live())
	    continue;

	// check name
	const char *ndata = ae->name.data();
	int nlen = ae->name.length();
	bool must_longname = false;
	for (int i = 0; i < nlen; i++)
	    if (ndata[i] == '/') {
		lerrh.error("archive element name %<%s%> contains slash",
			    ae->name.c_str());
		nlen = i;
		break;
	    } else if (isspace((unsigned char) ndata[i]))
		must_longname = true;

	// write name, or nameish thing
	if ((nlen >= 3 && ndata[0] == '#' && ndata[1] == '1' && ndata[2] == '/')
	    || must_longname || nlen > 14) {
	    if (char *x = sa.extend(16, 1))
		sprintf(x, "#1/%-13u", (unsigned)nlen);
	    must_longname = true;
	} else
	    if (char *x = sa.extend(16, 1))
		sprintf(x, "%-16.*s", nlen, ndata);

	// write other data
	int wrote_size = ae->data.length() + (must_longname ? nlen : 0);
	if (char *x = sa.extend(12 + 6 + 6 + 8 + 10 + 2, 1))
	    sprintf(x, "%-12u%-6u%-6u%-8o%-10u`\n", (unsigned) ae->date,
		    (unsigned) ae->uid, (unsigned) ae->gid, (unsigned) ae->mode,
		    (unsigned) wrote_size);

	if (must_longname)
	    sa.append(ndata, nlen);
	sa << ae->data;

	want_size += 60 + wrote_size;
	if (wrote_size % 2 == 1) {
	    // extend by an extra newline to keep object lengths even
	    sa << '\n';
	    want_size++;
	}
    }

    // check for memory errors
    if (want_size != sa.length()) {
	lerrh.error("out of memory");
	return String::make_out_of_memory();
    } else
	return sa.take_string();
}

CLICK_ENDDECLS
