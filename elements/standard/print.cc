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
extern "C" {
#include <linux/sched.h>
}
#endif

Print::Print()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

Print::~Print()
{
  MOD_DEC_USE_COUNT;
}

Print *
Print::clone() const
{
  return new Print;
}

int
Print::configure(Vector<String> &conf, ErrorHandler* errh)
{
  bool timestamp = false;
#ifdef CLICK_LINUXMODULE
  bool print_cpu = false;
#endif
  String label;
  unsigned bytes = 24;
  
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &label,
		  cpInteger, "max bytes to print", &bytes,
		  cpKeywords,
		  "NBYTES", cpInteger, "max bytes to print", &bytes,
		  "TIMESTAMP", cpBool, "print packet timestamps?", &timestamp,
#ifdef CLICK_LINUXMODULE
		  "CPU", cpBool, "print CPU IDs?", &print_cpu,
#endif
		  cpEnd) < 0)
    return -1;
  
  _label = label;
  _bytes = bytes;
  _timestamp = timestamp;
#ifdef CLICK_LINUXMODULE
  _cpu = print_cpu;
#endif
  return 0;
}

Packet *
Print::simple_action(Packet *p)
{
  StringAccum sa(3*_bytes + _label.length() + 45);
  if (sa.out_of_memory()) {
    click_chatter("no memory for Print");
    return p;
  }

  sa << _label;
#ifdef CLICK_LINUXMODULE
  if (_cpu)
    sa << '(' << current->processor << ')';
#endif
  if (_label)
    sa << ": ";
  if (_timestamp)
    sa << p->timestamp_anno() << ": ";

  // sa.reserve() must return non-null; we checked capacity above
  int len;
  len = sprintf(sa.reserve(9), "%4d | ", p->length());
  sa.forward(len);

  char *buf = sa.data() + sa.length();
  int pos = 0;
  for (unsigned i = 0; i < _bytes && i < p->length(); i++) {
    sprintf(buf + pos, "%02x", p->data()[i] & 0xff);
    pos += 2;
    if ((i % 4) == 3) buf[pos++] = ' ';
  }
  sa.forward(pos);

  click_chatter("%s", sa.cc());

  return p;
}

EXPORT_ELEMENT(Print)
ELEMENT_MT_SAFE(Print)
