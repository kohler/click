/*
 * RandomSource.{cc,hh} -- element generates random infinite stream
 * of packets
 * Robert Morris
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
#include "randomsource.hh"
#include "confparse.hh"
#include "error.hh"
#include "scheduleinfo.hh"
#include "glue.hh"

RandomSource::RandomSource()
{
  add_output();
}

RandomSource *
RandomSource::clone() const
{
  return new RandomSource;
}

int
RandomSource::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int length;
  
  if (cp_va_parse(conf, this, errh,
		  cpInteger, "packet length (bytes)", &length,
		  0) < 0)
    return -1;
  if(length < 0 || length >= 64*1024)
    return errh->error("bad length %d", length);
  _length = length;
  return 0;
}

int
RandomSource::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
RandomSource::uninitialize()
{
  unschedule();
}

void
RandomSource::run_scheduled()
{
  int i;
  WritablePacket *p = Packet::make(34, (const unsigned char *) 0,
                                   _length,
                                   Packet::default_tailroom(_length));
  char *d = (char *) p->data();
  
  for(i = 0; i < _length; i += sizeof(int))
    *(int*)(d + i) = random();
  for( ; i < _length; i++)
    *(d + i) = random();
  output(0).push(p);

  reschedule();
}

EXPORT_ELEMENT(RandomSource)
