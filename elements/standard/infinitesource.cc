/*
 * infinitesource.{cc,hh} -- element generates configurable infinite stream
 * of packets
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "infinitesource.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"
#include "glue.hh"

InfiniteSource::InfiniteSource()
  : _data("Random bullshit in a packet, at least 64 byte long.  Well, now it is."),
    _count(1), _limit(-1), _total(0)
{
  add_output();
}

InfiniteSource *
InfiniteSource::clone() const
{
  return new InfiniteSource;
}

int
InfiniteSource::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpOptional,
		     cpString, "packet data", &_data,
		     cpInteger, "total packet count", &_limit,
		     cpUnsigned, "packets per scheduling", &_count,
		     0);
}

int
InfiniteSource::initialize(ErrorHandler *)
{
  _packet = Packet::make(_data.data(), _data.length());
  return 0;
}

void
InfiniteSource::uninitialize()
{
  unschedule();
  _packet->kill();
  _packet = 0;
}

bool
InfiniteSource::run_scheduled()
{
  int count = _count;
  if (_limit >= 0 && _total + count >= _limit)
    count = _limit - _total;
  if (count <= 0)
    router()->please_stop_driver();
  else {
    for (int i = 0; i < count; i++)
      output(0).push(_packet->clone());
    _total += count;
    return true;
  }
  return false;
}

EXPORT_ELEMENT(InfiniteSource)
