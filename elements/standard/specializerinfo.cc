/*
 * scheduleinfo.{cc,hh} -- element stores schedule parameters
 * Benjie Chen, Eddie Kohler
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
#include "scheduleinfo.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"

ScheduleInfo::ScheduleInfo()
{
}

int
ScheduleInfo::configure(const String &conf, ErrorHandler *errh)
{
  return 0;
}

EXPORT_ELEMENT(SpecializerInfo)
