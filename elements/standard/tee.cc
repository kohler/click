#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "tee.hh"
#include "confparse.hh"
#include "error.hh"

Tee *
Tee::clone() const
{
  return new Tee(noutputs());
}

int
Tee::configure(const String &conf, ErrorHandler *errh)
{
  int n = noutputs();
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpUnsigned, "number of arms", &n,
		  0) < 0)
    return -1;
  add_outputs(n - noutputs());
  return 0;
}

void
Tee::push(int, Packet *p)
{
  int n = noutputs();
  for (int i = 0; i < n - 1; i++)
    output(i).push(p->clone());
  if (n > 0)
    output(n - 1).push(p);
  else
    p->kill();
}

//
// PULLTEE
//

void
PullTee::processing_vector(Vector<int> &in_v, int in_offset,
			     Vector<int> &out_v, int out_offset) const
{
  in_v[in_offset] = out_v[out_offset] = PULL;
  for (int o = 1; o < noutputs(); o++)
    out_v[out_offset+o] = PUSH;
}

PullTee *
PullTee::clone() const
{
  return new PullTee(noutputs());
}

int
PullTee::configure(const String &conf, ErrorHandler *errh)
{
  int n = noutputs();
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpUnsigned, "number of arms", &n,
		  0) < 0)
    return -1;
  if (n == 0)
    return errh->error("number of arms must be > 0");
  add_outputs(n - noutputs());
  return 0;
}

Packet *
PullTee::pull(int)
{
  Packet *p = input(0).pull();
  if (p) {
    int n = noutputs();
    for (int i = 1; i < n; i++)
      output(i).push(p->clone());
  }
  return p;
}

EXPORT_ELEMENT(Tee)
EXPORT_ELEMENT(PullTee)
