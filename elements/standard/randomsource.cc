/*
 * RandomSource.{cc,hh} -- element generates random infinite stream
 * of packets
 * Robert Morris
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
