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
#include "print.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"

Print::Print()
  : Element(1, 1)
{
  _buf = 0;
}

Print::~Print()
{
  delete[] _buf;
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
  delete[] _buf;
  _buf = new char[3*_bytes+1];
  if (_buf)
    return 0;
  else
    return errh->error("out of memory");
}

Packet *
Print::simple_action(Packet *p)
{
  int pos = 0;  
  for (unsigned i = 0; i < _bytes && i < p->length(); i++) {
    sprintf(_buf + pos, "%02x", p->data()[i] & 0xff);
    pos += 2;
    if ((i % 4) == 3) _buf[pos++] = ' ';
  }
  _buf[pos++] = '\0';
  click_chatter("Print %s |%4d : %s", _label.cc(), p->length(), _buf);
  return p;
}

EXPORT_ELEMENT(Print)
