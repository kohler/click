/*
 * perfcountinfo.{cc,hh} -- determine performance counters to be used
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "perfcountinfo.hh"
#include <click/args.hh>

PerfCountInfo::PerfCountInfo()
{
}

PerfCountInfo::~PerfCountInfo()
{
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
PerfCountInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String metric0, metric1;
  if (Args(conf, this, errh)
      .read_p("METRIC0", WordArg(), metric0)
      .read_p("METRIC1", WordArg(), metric1)
      .complete() < 0)
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
EXPORT_ELEMENT(PerfCountInfo)
