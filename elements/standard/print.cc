/*
 * print.{cc,hh} -- element prints packet contents to system log
 * John Jannotti, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "print.hh"
#include "glue.hh"
#include "confparse.hh"

Print::Print()
  : Element(1, 1)
{
  _buf = 0;
}

Print::Print(const String &label)
  : Element(1, 1), _label(label)
{
  _buf = 0;
}

Print::~Print()
{
  if (_buf) delete[] _buf;
}

Print *
Print::clone() const
{
  return new Print(_label);
}

int
Print::configure(const String &conf, ErrorHandler* errh)
{
  _bytes = 24;
  if (cp_va_parse(conf, this, errh,
		  cpString, "label", &_label,
		  cpOptional,
		  cpInteger, "max bytes to print", &_bytes,
		  cpEnd) < 0)
    return -1;
  _buf = new char[3*_bytes+1];
  return 0;
}

Packet *
Print::simple_action(Packet *p)
{
  if (!_buf)
    _buf = new char[3*_bytes+1];
  
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
