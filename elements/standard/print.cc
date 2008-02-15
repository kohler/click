/*
 * print.{cc,hh} -- element prints packet contents to system log
 * John Jannotti, Eddie Kohler
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
#include "print.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#ifdef CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/sched.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

Print::Print()
{
}

Print::~Print()
{
}

int
Print::configure(Vector<String> &conf, ErrorHandler* errh)
{
  bool timestamp = false;
#ifdef CLICK_LINUXMODULE
  bool print_cpu = false;
#endif
  bool print_anno = false, bcontents;
  String label, contents = "HEX";
  unsigned bytes = 24;
  
  if (cp_va_kparse(conf, this, errh,
		   "LABEL", cpkP, cpString, &label,
		   "LENGTH", cpkP, cpInteger, &bytes,
		   "NBYTES", 0, cpInteger, &bytes, // deprecated
		   "CONTENTS", 0, cpWord, &contents,
		   "TIMESTAMP", 0, cpBool, &timestamp,
		   "PRINTANNO", 0, cpBool, &print_anno,
#ifdef CLICK_LINUXMODULE
		   "CPU", 0, cpBool, &print_cpu,
#endif
		   cpEnd) < 0)
    return -1;

  if (cp_bool(contents, &bcontents))
      _contents = bcontents;
  else if ((contents = contents.upper()), contents == "NONE")
      _contents = 0;
  else if (contents == "HEX")
      _contents = 1;
  else if (contents == "ASCII")
      _contents = 2;
  else
      return errh->error("bad contents value '%s'; should be 'NONE', 'HEX', or 'ASCII'", contents.c_str());
  
  _label = label;
  _bytes = bytes;
  _timestamp = timestamp;
  _print_anno = print_anno;
#ifdef CLICK_LINUXMODULE
  _cpu = print_cpu;
#endif
  return 0;
}

Packet *
Print::simple_action(Packet *p)
{
    StringAccum sa(_label.length() + 2 // label:
		   + 6		// (processor)
		   + 28		// timestamp:
		   + 9		// length |
		   + Packet::USER_ANNO_SIZE*2 + 3 // annotations |
		   + 3 * _bytes);
    if (sa.out_of_memory()) {
	click_chatter("no memory for Print");
	return p;
    }

    const char *sep = "";
    if (_label) {
	sa << _label;
	sep = ": ";
    }
#ifdef CLICK_LINUXMODULE
    if (_cpu) {
	sa << '(' << click_current_processor() << ')';
	sep = ": ";
    }
#endif
    if (_timestamp) {
	sa << sep << p->timestamp_anno();
	sep = ": ";
    }

    // sa.reserve() must return non-null; we checked capacity above
    int len;
    len = sprintf(sa.reserve(11), "%s%4d", sep, p->length());
    sa.adjust_length(len);

  if (_print_anno) {
      sa << " | ";
      char *buf = sa.reserve(Packet::USER_ANNO_SIZE*2);
      int pos = 0;
      for (unsigned j = 0; j < Packet::USER_ANNO_SIZE; j++, pos += 2) 
	  sprintf(buf + pos, "%02x", p->user_anno_u8(j));
      sa.adjust_length(pos);
  }

    if (_contents) {
	sa << " | ";
	char *buf = sa.data() + sa.length();
	const unsigned char *data = p->data();
	if (_contents == 1) {
	    for (unsigned i = 0; i < _bytes && i < p->length(); i++, data++) {
		sprintf(buf, "%02x", *data & 0xff);
		buf += 2;
		if ((i % 4) == 3) *buf++ = ' ';
	    }
	} else if (_contents == 2) {
	    for (unsigned i = 0; i < _bytes && data < p->end_data(); i++, data++) {
		if ((i % 8) == 0)
		    *buf++ = ' ';
		if (*data < 32 || *data > 126)
		    *buf++ = '.';
		else
		    *buf++ = *data;
	    }
	}
	sa.adjust_length(buf - (sa.data() + sa.length()));
    }

  click_chatter("%s", sa.c_str());

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Print)
ELEMENT_MT_SAFE(Print)
