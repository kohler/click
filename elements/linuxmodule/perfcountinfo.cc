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
EXPORT_ELEMENT(PerfCountInfo)
