/*
 * perfcountinfo.{cc,hh} -- determine performance counters to be used
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "perfcountinfo.hh"
#include <click/confparse.hh>

PerfCountInfo::PerfCountInfo()
{
  MOD_INC_USE_COUNT;
}

PerfCountInfo::~PerfCountInfo()
{
  MOD_DEC_USE_COUNT;
}

void *
PerfCountInfo::cast(const char *n)
{
  if (strcmp(n, "PerfCountUser") == 0)
    return (PerfCountUser *)this;
  else if (strcmp(n, "PerfCountInfo") == 0)
    return (Element *)this;
  else
    return 0;
}

int
PerfCountInfo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String metric0, metric1;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpWord, "performance metric 0", &metric0,
		  cpWord, "performance metric 1", &metric1,
		  cpEnd) < 0)
    return -1;
  
  bool ok = true;
  if (metric0) {
    if (PerfCountUser::prepare(metric0, errh, 0) < 0)
      ok = false;
  }
  if (metric1) {
    if (PerfCountUser::prepare(metric1, errh, 1) < 0)
      ok = false;
  }
  
  return (ok ? 0 : -1);
}

ELEMENT_REQUIRES(linuxmodule PerfCountUser)
ELEMENT_PROVIDES(PerfCountInfo)
