/*
 * locationinfo.{cc,hh} -- element gives the grid node's current location
 * Douglas S. J. De Couto
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
#include "locationinfo.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"

LocationInfo::LocationInfo() : _loc(32.2816, -64.7685) // my house in bermuda -- doug
{
}

LocationInfo::~LocationInfo()
{
}

EXPORT_ELEMENT(LocationInfo)
