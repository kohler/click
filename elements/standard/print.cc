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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "print.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
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
  _bytes = 24;
  if (cp_va_parse(conf, this, errh,
		  cpString, "label", &_label,
		  cpOptional,
		  cpInteger, "max bytes to print", &_bytes,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
Print::simple_action(Packet *p)
{
  char *buf = new char[3*_bytes+1];
  if (!buf) {
    click_chatter("no memory for Print");
    return p;
  }
  int pos = 0;  
  for (unsigned i = 0; i < _bytes && i < p->length(); i++) {
    sprintf(buf + pos, "%02x", p->data()[i] & 0xff);
    pos += 2;
    if ((i % 4) == 3) buf[pos++] = ' ';
  }
  buf[pos++] = '\0';
#ifdef __KERNEL__
  click_chatter("Print %s (%d) |%4d : %s", 
                _label.cc(), current->processor, p->length(), buf);
#else
  click_chatter("Print %s |%4d : %s", _label.cc(), p->length(), buf);
#endif
  delete[] buf;
  return p;
}

EXPORT_ELEMENT(Print)
ELEMENT_MT_SAFE(Print)

