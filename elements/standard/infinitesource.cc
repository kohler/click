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
InfiniteSource::configure(const String &conf, Router *r, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, r, errh,
		     cpUnsigned, "packets per scheduling", &_count,
		     cpOptional,
		     cpString, "packet data", &_data,
		     cpInteger, "total packet count", &_limit,
		     0);
}

int
InfiniteSource::initialize(Router *, ErrorHandler *)
{
  schedule_tail();
  return 0;
}

void
InfiniteSource::uninitialize(Router *)
{
  unschedule();
}

void
InfiniteSource::run_scheduled()
{
  if (_limit >= 0 && _total >= _limit)
    router()->please_stop_driver();
  else {
    for (int i = 0; i < _count; i++)
      output(0).push(Packet::make(_data.data(), _data.length()));
    _total += _count;
    schedule_tail();
  }
}

EXPORT_ELEMENT(InfiniteSource)
