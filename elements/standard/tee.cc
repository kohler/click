/*
 * tee.{cc,hh} -- element duplicates packets
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
#include "tee.hh"
#include "confparse.hh"
#include "error.hh"

Tee *
Tee::clone() const
{
  return new Tee;
}

void
Tee::notify_noutputs(int n)
{
  set_noutputs(n < 1 ? 1 : n);
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
  if (n < 1)
    return errh->error("number of arms must be at least 1");
  set_noutputs(n);
  return 0;
}

void
Tee::push(int, Packet *p)
{
  int n = noutputs();
  for (int i = 0; i < n - 1; i++)
    output(i).push(p->clone());
  output(n - 1).push(p);
}

//
// PULLTEE
//

PullTee *
PullTee::clone() const
{
  return new PullTee;
}

void
PullTee::notify_noutputs(int n)
{
  set_noutputs(n < 1 ? 1 : n);
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
  if (n < 1)
    return errh->error("number of arms must be at least 1");
  set_noutputs(n - noutputs());
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
