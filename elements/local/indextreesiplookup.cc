/*
 * indextreesiplookup.{cc,hh} -- looks up next-hop address in index of trees
 * Benjie Chen
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, subject to the conditions listed in the Click LICENSE
 * file. These conditions include: you must preserve this copyright
 * notice, and you cannot mention the copyright holders in advertising
 * related to the Software without their permission.  The Software is
 * provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This notice is a
 * summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "indextreesiplookup.hh"
CLICK_DECLS

IndexTreesIPLookup::IndexTreesIPLookup()
{
  for (int i=0; i<INDEX_SIZE; i++)
    _trees[i] = 0;
}

IndexTreesIPLookup::~IndexTreesIPLookup()
{
}

void
IndexTreesIPLookup::cleanup(CleanupStage)
{
  for (int i=0; i<INDEX_SIZE; i++)
    delete _trees[i];
}

String
IndexTreesIPLookup::dump_routes()
{
  String ret = "";
  return ret;
}

void
IndexTreesIPLookup::add_route(IPAddress d, IPAddress m, IPAddress g, int port)
{
  /* need to add entry to every root that could potentially match */
  IPAddress end = ;
}

void
IndexTreesIPLookup::remove_route(IPAddress d, IPAddress m)
{
  /* need to remove entry from every root that could potentially match */
}

int
IndexTreesIPLookup::lookup_route(IPAddress d, IPAddress &gw)
{
  int h = hash(d);
}

CLICK_ENDDECLS
// EXPORT_ELEMENT(IndexTreesIPLookup)
