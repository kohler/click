/*
 * meter.{cc,hh} -- element sends packets out one of several outputs
 * depending on recent rate (bytes/s)
 * Eddie Kohler
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
#include "meter.hh"
#include "error.hh"
#include "router.hh"
#include "confparse.hh"
#include <errno.h>

Meter::Meter()
  : Element(1, 1), _meters(0), _nmeters(0)
{
}

Meter::~Meter()
{
  delete[] _meters;
}

Meter *
Meter::clone() const
{
  return new Meter;
}

int
Meter::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  
  delete[] _meters;
  _meters = 0;
  _nmeters = 0;

  if (args.size() == 0)
    return errh->error("too few arguments to Meter(int, ...)");

  Vector<int> vals(args.size(), 0);
  for (int i = 0; i < args.size(); i++)
    if (!cp_integer(args[i], vals[i]))
      return errh->error("argument %d should be int (rate)", i+1);
    else if (vals[i] <= 0)
      return errh->error("argument %d (rate) must be >= 0", i+1);
    else if (i > 0 && vals[i] <= vals[i-1])
      return errh->error("rate %d must be > rate %d", i+1, i);
  
  int max_value = ((0xFFFFFFFF<<_rate.scale()) & ~0x80000000);
  for (int i = 0; i < args.size(); i++) {
    if (vals[i] > max_value)
      return errh->error("rate %d too large (max %d)", i+1, max_value);
    vals[i] = (vals[i]<<_rate.scale()) / CLICK_HZ;
  }
  
  if (vals.size() == 1) {
    _meter1 = vals[0];
    _nmeters = 1;
  } else {
    _meters = new int[vals.size()];
    memcpy(_meters, &vals[0], vals.size() * sizeof(int));
    _nmeters = vals.size();
  }

  set_noutputs(_nmeters + 1);
  return 0;
}

int
Meter::initialize(ErrorHandler *)
{
  _rate.initialize();
  return 0;
}

void
Meter::push(int, Packet *p)
{
  _rate.update(p->length());

  int r = _rate.average();
  if (_nmeters < 2) {
    int n = (r >= _meter1);
    output(n).push(p);
  } else {
    int *meters = _meters;
    int nmeters = _nmeters;
    for (int i = 0; i < nmeters; i++)
      if (r < meters[i]) {
	output(i).push(p);
	return;
      }
    output(nmeters).push(p);
  }
}

String
Meter::meters_read_handler(Element *f, void *)
{
  Meter *m = (Meter *)f;
  if (m->_nmeters == 1)
    return cp_unparse_real(m->_meter1*CLICK_HZ, m->rate_scale()) + "\n";
  else {
    String s;
    for (int i = 0; i < m->_nmeters; i++)
      s = s + cp_unparse_real(m->_meters[i]*CLICK_HZ, m->rate_scale()) + "\n";
    return s;
  }
}

static String
read_rate_handler(Element *f, void *)
{
  Meter *c = (Meter *)f;
  return cp_unparse_real(c->rate()*CLICK_HZ, c->rate_scale()) + "\n";
}

void
Meter::add_handlers()
{
  add_read_handler("rate", read_rate_handler, 0);
  add_read_handler("meters", Meter::meters_read_handler, 0);
}

EXPORT_ELEMENT(Meter)
