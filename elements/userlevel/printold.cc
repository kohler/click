/*
 * printold.{cc,hh} -- element prints ``old'' packet contents to system log
 * Douglas S. J. De Couto
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
#include "printold.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>

PrintOld::PrintOld()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

PrintOld::~PrintOld()
{
  MOD_DEC_USE_COUNT;
}

PrintOld *
PrintOld::clone() const
{
  return new PrintOld;
}

int
PrintOld::configure(const Vector<String> &conf, ErrorHandler* errh)
{
  _label = String();
  _bytes = 24;
  _thresh = 5;
  
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &_label,
		  cpInteger, "age threshold (milliseconds)", &_thresh,
		  cpInteger, "max bytes to print", &_bytes,
		  cpEnd) < 0)
    return -1;
  
  return 0;
}

Packet *
PrintOld::simple_action(Packet *p)
{
  StringAccum sa(3*_bytes + _label.length() + 55);
  if (!sa.capacity()) {
    click_chatter("no memory for PrintOld");
    return p;
  }

  struct timeval tv_now;
  int res = gettimeofday(&tv_now, 0);
  if (!res) {
    click_chatter("%s: unable to get time of day", id().cc());
    return p;
  }

  long age_s = p->timestamp_anno().tv_sec - tv_now.tv_sec;
  long age_u = p->timestamp_anno().tv_usec - tv_now.tv_usec;

  if (age_s < 0) {
    click_chatter("%s: packet timestamp not set", id().cc());
    return p;
  }

  // skankyness... 
  long age_ms = age_s * 1000 + age_u / 1000;
  if (age_ms < _thresh)
    return p;

  // else print it...

  sa << _label;
  
  // sa.reserve() must return non-null; we checked capacity above
  int len;
  sprintf(sa.reserve(9), "(%5ld msecs) %4d | %n", age_ms, p->length(), &len);
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

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(PrintOld)

