/*
 * settimestamp.{cc,hh} -- set timestamp annotations
 * Douglas S. J. De Couto
 * based on setperfcount.{cc,hh}
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
#include "settimestamp.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

SetTimestamp::SetTimestamp()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

SetTimestamp::~SetTimestamp()
{
  MOD_DEC_USE_COUNT;
}

SetTimestamp *
SetTimestamp::clone() const
{
  return new SetTimestamp();
}

int
SetTimestamp::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _tv.tv_sec = -1;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpTimeval, "timestamp", &_tv,
		  cpEnd) < 0)
    return -1;
  return 0;
}

inline void
SetTimestamp::smaction(Packet *p)
{
  struct timeval &tv = p->timestamp_anno();
  if (_tv.tv_sec >= 0)
    tv = _tv;
  else
    click_gettimeofday(&tv);
}

void
SetTimestamp::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
SetTimestamp::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    smaction(p);
  return p;
}

EXPORT_ELEMENT(SetTimestamp)
