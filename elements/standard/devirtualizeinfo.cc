/*
 * devirtualizeinfo.{cc,hh} -- element stores devirtualization parameters
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "devirtualizeinfo.hh"
#include "glue.hh"

DevirtualizeInfo::DevirtualizeInfo()
{
}

int
DevirtualizeInfo::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}

EXPORT_ELEMENT(DevirtualizeInfo)
