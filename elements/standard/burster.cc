#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "burster.hh"
#include "confparse.hh"
#include "error.hh"

Burster::Burster()
  : _npackets(0), _timer(this)
{
  add_input();
  add_output();
}

Burster::~Burster()
{
}

Burster *
Burster::clone() const
{
  return new Burster;
}

int
Burster::configure(const String &conf, Router *r, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, r, errh,
		  cpInterval, "packet pulling interval", &_interval,
		  cpOptional,
		  cpUnsigned, "max packets per interval", &_npackets,
		  0) < 0)
    return -1;
  if (_npackets <= 0)
    return errh->error("max packets per interval must be > 0");
  return 0;
}

int
Burster::initialize(Router *, ErrorHandler *)
{
  _timer.schedule_after_ms(_interval);
  return 0;
}

void
Burster::uninitialize(Router *)
{
  _timer.unschedule();
}

bool
Burster::wants_packet_upstream() const
{
  return true;
}

void
Burster::run_scheduled()
{
  // don't run if the timer is scheduled (an upstream queue went empty but we
  // don't care)
  if (_timer.scheduled())
    return;
  
  for (int i = 0; i < _npackets; i++) {
    Packet *p = input(0).pull();
    if (!p)
      // no packets left; return w/o resetting timer. rely on PACKET_UPSTREAM
      // scheduling to wake us up later
      return;
    output(0).push(p);
  }

  // reset timer
  _timer.schedule_after_ms(_interval);
}

EXPORT_ELEMENT(Burster)
