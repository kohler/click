/*
 * gridheaderinfo.{cc,hh} -- Provide information about Grid header sizes and offsets
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
 * legally binding.  */

#include <click/config.h>
#include "elements/grid/gridheaderinfo.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include "elements/grid/linkstat.hh"
CLICK_DECLS

/*
 * At this point, I have to ask myself, ``is this really making your
 * life easier?''.  Also, of course, WWRD (what would Roger do?).
 */

static const GridHeaderInfo::info_t
handler_info_array[] = {
  { "grid_hdr_version"               , grid_hdr::GRID_VERSION           , 'h', -1 },

  { "grid_ether_proto"               , ETHERTYPE_GRID                   , 'h',  4 },

  { "grid_proto_hello"               , grid_hdr::GRID_HELLO             , 'h',  2 },
  { "grid_proto_lr_hello"            , grid_hdr::GRID_LR_HELLO          , 'h',  2 },
  { "grid_proto_nbr_encap"           , grid_hdr::GRID_NBR_ENCAP         , 'h',  2 },
  { "grid_proto_loc_query"           , grid_hdr::GRID_LOC_QUERY         , 'h',  2 },
  { "grid_proto_loc_reply"           , grid_hdr::GRID_LOC_REPLY         , 'h',  2 },
  { "grid_proto_route_probe"         , grid_hdr::GRID_ROUTE_PROBE       , 'h',  2 },
  { "grid_proto_route_reply"         , grid_hdr::GRID_ROUTE_REPLY       , 'h',  2 },
  { "grid_proto_geocast"             , grid_hdr::GRID_GEOCAST           , 'h',  2 },
  { "grid_proto_link_probe"          , grid_hdr::GRID_LINK_PROBE        , 'h',  2 },

  { "sizeof_grid_location"           , sizeof(grid_location)            , 'd', -1 },
  { "sizeof_grid_hdr"                , sizeof(grid_hdr)			, 'd', -1 },
  { "sizeof_grid_nbr_entry"          , sizeof(grid_nbr_entry)		, 'd', -1 },
  { "sizeof_grid_hello"              , sizeof(grid_hello)		, 'd', -1 },
  { "sizeof_grid_nbr_encap"          , sizeof(grid_nbr_encap)		, 'd', -1 },
  { "sizeof_grid_loc_query"          , sizeof(grid_loc_query)		, 'd', -1 },
  { "sizeof_grid_route_probe"        , sizeof(grid_route_probe)		, 'd', -1 },
  { "sizeof_grid_route_reply"        , sizeof(grid_route_reply)		, 'd', -1 },
  { "sizeof_grid_link_probe"         , sizeof(grid_link_probe)		, 'd', -1 },
  { "sizeof_grid_link_entry"         , sizeof(grid_link_entry)		, 'd', -1 },

  { "sizeof_linkstat_link_probe"     , LinkStat::link_probe::size       , 'd', -1 },
  { "sizeof_linkstat_link_entry"     , LinkStat::link_entry::size       , 'd', -1 },

  { "offsetof_grid_hdr_version"      , offsetof(grid_hdr, version)	, 'd', -1 },
  { "offsetof_grid_hdr_type"         , offsetof(grid_hdr, type)		, 'd', -1 },
  { "offsetof_grid_hdr_ip"           , offsetof(grid_hdr, ip)		, 'd', -1 },
  { "offsetof_grid_hdr_tx_ip"        , offsetof(grid_hdr, tx_ip)	, 'd', -1 },
  { "offsetof_grid_hdr_total_len"    , offsetof(grid_hdr, total_len)	, 'd', -1 },

  { "offsetof_grid_nbr_encap_dst_ip" , offsetof(grid_nbr_encap, dst_ip)	, 'd', -1 },

  { "offsetof_grid_loc_query_dst_ip" , offsetof(grid_loc_query, dst_ip) , 'd', -1 },
};



GridHeaderInfo::GridHeaderInfo()
{
}

GridHeaderInfo::~GridHeaderInfo()
{
}

int
GridHeaderInfo::read_args(const Vector<String> &, ErrorHandler *)
{
  return 0;
}
int
GridHeaderInfo::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}


static String
ghi_read_handler(Element *, void *v)
{
  int num_handlers = sizeof(handler_info_array) / sizeof(GridHeaderInfo::info_t);
  intptr_t handler_index = (intptr_t) v;
  if (handler_index < 0 || handler_index >= num_handlers)
    return String("An non-existent handler was specified\n");

  GridHeaderInfo::info_t info = handler_info_array[handler_index];

  /* Gross */
  char fmt[10];
  if (info.width > 0)
    snprintf(fmt, sizeof(fmt), "%%0%d%c", info.width, (info.base == 'h') ? 'x' : 'u');
  else
    snprintf(fmt, sizeof(fmt), "%%%c", (info.base == 'h') ? 'x' : 'u');

  char buf[80];
  snprintf(buf, sizeof(buf), fmt, info.val);

  return String(buf) + "\n";
}




void
GridHeaderInfo::add_handlers()
{
  for (unsigned int i = 0; i < sizeof(handler_info_array)/sizeof(GridHeaderInfo::info_t); i++)
    add_read_handler(handler_info_array[i].name, ghi_read_handler, i);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GridHeaderInfo)
