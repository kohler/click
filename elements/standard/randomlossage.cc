#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "randomlossage.hh"
#include "confparse.hh"
#include "error.hh"

RandomLossage::RandomLossage(int p_drop, bool on)
  : Element(1, 1), _p_drop(p_drop), _on(on)
{
}

RandomLossage *
RandomLossage::clone() const
{
  return new RandomLossage(_p_drop, _on);
}

void
RandomLossage::notify_outputs(int n)
{
  n = (n >= 2 ? 2 : 1);
  add_outputs(n - noutputs());
}

void
RandomLossage::processing_vector(Vector<int> &in_v, int in_offset,
				   Vector<int> &out_v, int out_offset) const
{
  in_v[in_offset+0] = out_v[out_offset+0] = AGNOSTIC;
  if (noutputs() == 2)
    out_v[out_offset+1] = PUSH;
}

int
RandomLossage::configure(const String &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpNonnegReal2, "max_p drop probability", 16, &_p_drop,
		  cpOptional,
		  cpBool, "active?", &_on,
		  0) < 0)
    return -1;
  if (_p_drop > 0x10000)
    return errh->error("drop probability must be between 0 and 1");
  return 0;
}

int
RandomLossage::initialize(ErrorHandler *errh)
{
  if (_p_drop < 0)
    return errh->error("not configured");
  _drops = 0;
  return 0;
}

void
RandomLossage::push(int, Packet *p)
{
  if (!_on || ((random()>>5) & 0xFFFF) >= _p_drop)
    output(0).push(p);
  else if (noutputs() == 2) {
    output(1).push(p);
    _drops++;
  } else {
    p->kill();
    _drops++;
  }
}

Packet *
RandomLossage::pull(int)
{
  Packet *p = input(0).pull();
  if (!p)
    return 0;
  else if (!_on || ((random()>>5) & 0xFFFF) >= _p_drop)
    return p;
  else if (noutputs() == 2) {
    output(1).push(p);
    _drops++;
    return 0;
  } else {
    p->kill();
    _drops++;
    return 0;
  }
}

static String
random_lossage_read(Element *f, void *vwhich)
{
  int which = (int)vwhich;
  RandomLossage *lossage = (RandomLossage *)f;
  if (which == 0)
    return cp_unparse_real(lossage->p_drop(), 16) + "\n";
  else if (which == 1)
    return (lossage->on() ? "true\n" : "false\n");
  else
    return String(lossage->drops()) + "\n";
}

void
RandomLossage::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read_write("p_drop", random_lossage_read, (void *)0,
		      reconfigure_write_handler, (void *)0);
  fcr->add_read_write("active", random_lossage_read, (void *)1,
		      reconfigure_write_handler, (void *)1);
  fcr->add_read("drops", random_lossage_read, (void *)2);
}

EXPORT_ELEMENT(RandomLossage)
