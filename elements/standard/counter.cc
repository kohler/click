#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "counter.hh"
#include "confparse.hh"

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
Counter::initialize(ErrorHandler *)
{
  reset();
  return 0;
}

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


static String
counter_read_count_handler(Element *f, void *)
{
  Counter *c = (Counter *)f;
  return String(c->count()) + "\n";
}

static String
counter_read_rate_handler(Element *f, void *)
{
  Counter *c = (Counter *)f;
  return cp_unparse_real(c->rate()*HZ, c->rate_scale()) + "\n";
}

static int
counter_reset_write_handler(Element *f, const String &, void *, ErrorHandler *)
{
  Counter *c = (Counter *)f;
  c->reset();
  return 0;
}

void
Counter::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read("count", counter_read_count_handler, 0);
  fcr->add_read("rate", counter_read_rate_handler, 0);
  fcr->add_write("reset", counter_reset_write_handler, 0);
}


EXPORT_ELEMENT(Counter)
