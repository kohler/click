/*
 * tee.{cc,hh} -- element duplicates packets
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
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
    output(i).push(p->clone());
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
      output(i).push(p->clone());
  }
  return p;
}

EXPORT_ELEMENT(Tee)
EXPORT_ELEMENT(PullTee)
