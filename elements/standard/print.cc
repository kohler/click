/*
 * print.{cc,hh} -- element prints packet contents to system log
 * John Jannotti, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/package.hh>
#include "print.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#ifdef __KERNEL__
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
Print::configure(const Vector<String> &conf, ErrorHandler* errh)
{
  bool timestamp = false;
#ifdef __KERNEL__
  bool print_cpu = false;
#endif
  _label = String();
  _bytes = 24;
  
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &_label,
		  cpInteger, "max bytes to print", &_bytes,
		  cpKeywords,
		  "NBYTES", cpInteger, "max bytes to print", &_bytes,
		  "TIMESTAMP", cpBool, "print packet timestamps?", &timestamp,
#ifdef __KERNEL__
		  "CPU", cpBool, "print CPU IDs?", &print_cpu,
#endif
		  cpEnd) < 0)
    return -1;
  
  _timestamp = timestamp;
#ifdef __KERNEL__
  _cpu = print_cpu;
#endif
  return 0;
}

Packet *
Print::simple_action(Packet *p)
{
  StringAccum sa(3*_bytes + _label.length() + 45);
  if (!sa.capacity()) {
    click_chatter("no memory for Print");
    return p;
  }

  sa << _label;
#ifdef __KERNEL__
  if (_cpu)
    sa << '(' << current->processor << ')';
#endif
  if (_label)
    sa << ": ";
  if (_timestamp)
    sa << p->timestamp_anno() << ": ";

  // sa.reserve() must return non-null; we checked capacity above
  int len;
  sprintf(sa.reserve(9), "%4d | %n", p->length(), &len);
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
