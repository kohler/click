/*
 * bandwidthmeter.{cc,hh} -- element sends packets out one of several outputs
 * depending on recent rate (bytes/s)
 * Eddie Kohler
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
#include "bandwidthmeter.hh"
#include <click/error.hh>
#include <click/confparse.hh>
CLICK_DECLS

BandwidthMeter::BandwidthMeter()
  : Element(1, 1), _meters(0), _nmeters(0)
{
  MOD_INC_USE_COUNT;
}

BandwidthMeter::~BandwidthMeter()
{
  MOD_DEC_USE_COUNT;
  delete[] _meters;
}

BandwidthMeter *
BandwidthMeter::clone() const
{
  return new BandwidthMeter;
}

int
BandwidthMeter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  delete[] _meters;
  _meters = 0;
  _nmeters = 0;

  if (conf.size() == 0)
    return errh->error("too few arguments to BandwidthMeter(int, ...)");

  Vector<unsigned> vals(conf.size(), 0);
  for (int i = 0; i < conf.size(); i++)
    if (!cp_unsigned(conf[i], &vals[i]))
      return errh->error("argument %d should be unsigned (rate)", i+1);
    else if (i > 0 && vals[i] <= vals[i-1])
      return errh->error("rate %d must be > rate %d", i+1, i);

  unsigned max_value = 0xFFFFFFFF >> _rate.scale;
  for (int i = 0; i < conf.size(); i++) {
    if (vals[i] > max_value)
      return errh->error("rate %d too large (max %u)", i+1, max_value);
    vals[i] = (vals[i]<<_rate.scale) / _rate.freq();
  }
  
  if (vals.size() == 1) {
    _meter1 = vals[0];
    _nmeters = 1;
  } else {
    _meters = new unsigned[vals.size()];
    memcpy(_meters, &vals[0], vals.size() * sizeof(int));
    _nmeters = vals.size();
  }

  set_noutputs(_nmeters + 1);
  return 0;
}

int
BandwidthMeter::initialize(ErrorHandler *)
{
  _rate.initialize();
  return 0;
}

void
BandwidthMeter::push(int, Packet *p)
{
  _rate.update(p->length());

  unsigned r = _rate.average();
  if (_nmeters < 2) {
    int n = (r >= _meter1);
    output(n).push(p);
  } else {
    unsigned *meters = _meters;
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
BandwidthMeter::meters_read_handler(Element *f, void *)
{
  BandwidthMeter *m = (BandwidthMeter *)f;
  if (m->_nmeters == 1)
    return cp_unparse_real2(m->_meter1*m->rate_freq(), m->rate_scale()) + "\n";
  else {
    String s;
    for (int i = 0; i < m->_nmeters; i++)
      s = s + cp_unparse_real2(m->_meters[i]*m->rate_freq(), m->rate_scale()) + "\n";
    return s;
  }
}

static String
read_rate_handler(Element *f, void *)
{
  BandwidthMeter *c = (BandwidthMeter *)f;
  return cp_unparse_real2(c->rate()*c->rate_freq(), c->rate_scale()) + "\n";
}

void
BandwidthMeter::add_handlers()
{
  add_read_handler("rate", read_rate_handler, 0);
  add_read_handler("meters", BandwidthMeter::meters_read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BandwidthMeter)
