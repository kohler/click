/*
 * counter.{cc,hh} -- element counts packets, measures packet rate
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
#include "counter.hh"
#include "confparse.hh"
#include "error.hh"

static String counter_read_rate_handler(Element *, void *);

Counter::Counter()
  : Element(1, 1), _count(0)
{
}

void
Counter::reset()
{
  _count = 0;
  _rate.initialize();
}

int
Counter::configure(const Vector<String> &conf, ErrorHandler *errh) 
{ 
  _bytes = false;
  String b = "PACKETS";
  if (cp_va_parse(conf, this, errh, 
		  cpOptional,
	          cpString, "count bytes?", &b,
		  0) < 0)
    return -1;
  if (b == "BYTES") _bytes = true;
  else if (b == "PACKETS") _bytes = false;
  else 
    return errh->error("argument should be BYTES or PACKETS");
  return 0;
}


int
Counter::initialize(ErrorHandler *)
{
  reset();
  return 0;
}

Packet *
Counter::simple_action(Packet *p)
{
  if (!_bytes) {
    _count++;
    _rate.update(1);
  } else {
    _count += p->length();
    _rate.update(p->length());
  }
  return p;
}

/*
void
Counter::push(int, Packet *packet)
{
  _count++;
  _rate.update(1);
  output(0).push(packet);
}

Packet *
Counter::pull(int)
{
  Packet *p = input(0).pull();
  if (p) {
    _count++;
    _rate.update(1);
  }
  return p;
}
*/

static String
counter_read_count_handler(Element *e, void *)
{
  Counter *c = (Counter *)e;
  return String(c->count()) + "\n";
}

static String
counter_read_rate_handler(Element *e, void *)
{
  Counter *c = (Counter *)e;
  return cp_unparse_real(c->rate()*c->rate_freq(), c->rate_scale()) + "\n";
}

static int
counter_reset_write_handler(const String &, Element *e, void *, ErrorHandler *)
{
  Counter *c = (Counter *)e;
  c->reset();
  return 0;
}

void
Counter::add_handlers()
{
  add_read_handler("count", counter_read_count_handler, 0);
  add_read_handler("rate", counter_read_rate_handler, 0);
  add_write_handler("reset", counter_reset_write_handler, 0);
}


EXPORT_ELEMENT(Counter)
