/*
 * tee.{cc,hh} -- element duplicates packets
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
#include "tee.hh"
#include <click/confparse.hh>
#include <click/error.hh>

Tee::Tee()
{
  MOD_INC_USE_COUNT;
  add_input();
}

Tee::~Tee()
{
  MOD_DEC_USE_COUNT;
}

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
Tee::configure(const Vector<String> &conf, ErrorHandler *errh)
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
    if (Packet *q = p->clone())
      output(i).push(q);
  output(n - 1).push(p);
}

//
// PULLTEE
//

PullTee::PullTee()
{
  MOD_INC_USE_COUNT;
  add_input();
}

PullTee::~PullTee()
{
  MOD_DEC_USE_COUNT;
}

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
PullTee::configure(const Vector<String> &conf, ErrorHandler *errh)
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

Packet *
PullTee::pull(int)
{
  Packet *p = input(0).pull();
  if (p) {
    int n = noutputs();
    for (int i = 1; i < n; i++)
      if (Packet *q = p->clone())
	output(i).push(q);
  }
  return p;
}

EXPORT_ELEMENT(Tee PullTee)
ELEMENT_MT_SAFE(Tee)
ELEMENT_MT_SAFE(PullTee)
