/*
 * devirtualizeinfo.{cc,hh} -- element stores devirtualization parameters
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

#include <click/config.h>
#include <click/package.hh>
#include "devirtualizeinfo.hh"
#include <click/glue.hh>

DevirtualizeInfo::DevirtualizeInfo()
{
  MOD_INC_USE_COUNT;
}

DevirtualizeInfo::~DevirtualizeInfo()
{
  MOD_DEC_USE_COUNT;
}

int
DevirtualizeInfo::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}

EXPORT_ELEMENT(DevirtualizeInfo)
