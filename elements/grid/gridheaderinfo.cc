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
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>

GridHeaderInfo::GridHeaderInfo()
{
  MOD_INC_USE_COUNT;
}

GridHeaderInfo::~GridHeaderInfo()
{
  MOD_DEC_USE_COUNT;
}

int
GridHeaderInfo::read_args(const Vector<String> &, ErrorHandler *)
{
  return 0;
}
int
GridHeaderInfo::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}


static String
ghi_read_handler(Element *, void *v)
{
  int what = (int) v;
  int answer;
  char buf[80];

  switch (what) {

  case GridHeaderInfo::grid_hdr_version:               
    snprintf(buf, sizeof(buf), "0x%x\n", grid_hdr::GRID_VERSION);
    return buf;
    break;
       
  case GridHeaderInfo::sizeof_grid_location:           answer = sizeof(grid_location); break;
  case GridHeaderInfo::sizeof_grid_hdr:                answer = sizeof(grid_hdr); break;
  case GridHeaderInfo::sizeof_grid_nbr_entry:          answer = sizeof(grid_nbr_entry); break;
  case GridHeaderInfo::sizeof_grid_hello:              answer = sizeof(grid_hello); break;
  case GridHeaderInfo::sizeof_grid_nbr_encap:          answer = sizeof(grid_nbr_encap); break;
  case GridHeaderInfo::sizeof_grid_loc_query:          answer = sizeof(grid_loc_query); break;
  case GridHeaderInfo::sizeof_grid_route_probe:        answer = sizeof(grid_route_probe); break;
  case GridHeaderInfo::sizeof_grid_route_reply:        answer = sizeof(grid_route_reply); break;
       
  case GridHeaderInfo::offsetof_grid_hdr_version:      answer = offsetof(grid_hdr, version); break;
  case GridHeaderInfo::offsetof_grid_hdr_type:         answer = sizeof(grid_hdr); break;
  case GridHeaderInfo::offsetof_grid_hdr_ip:           answer = sizeof(grid_hdr); break;
  case GridHeaderInfo::offsetof_grid_hdr_tx_ip:        answer = sizeof(grid_hdr); break;
       
  case GridHeaderInfo::offsetof_grid_nbr_encap_dst_ip: answer = sizeof(grid_hdr); break;
       
  case GridHeaderInfo::offsetof_grid_loc_query_dst_ip: answer = sizeof(grid_hdr); break;
      default: answer = -1;
  }
  return String(answer) + "\n";  
}




void
GridHeaderInfo::add_handlers()
{
  add_default_handlers(true);
 
  /*
   * surely there is a better way to implement all of these handlers,
   * and this just shows i am a dumb hack when it comes to C++.  If
   * you have a better idea, please inform me!  decouto. 
   */

  add_read_handler("grid_hdr_version"                , ghi_read_handler, (void *)     grid_hdr_version             );	 
  
  add_read_handler("sizeof_grid_location"	     , ghi_read_handler, (void *)     sizeof_grid_location         );
  add_read_handler("sizeof_grid_hdr"		     , ghi_read_handler, (void *)     sizeof_grid_hdr              );
  add_read_handler("sizeof_grid_nbr_entry"	     , ghi_read_handler, (void *)     sizeof_grid_nbr_entry        );
  add_read_handler("sizeof_grid_hello"	             , ghi_read_handler, (void *)     sizeof_grid_hello            );
  add_read_handler("sizeof_grid_nbr_encap"	     , ghi_read_handler, (void *)     sizeof_grid_nbr_encap        );
  add_read_handler("sizeof_grid_loc_query"	     , ghi_read_handler, (void *)     sizeof_grid_loc_query        );
  add_read_handler("sizeof_grid_route_probe"	     , ghi_read_handler, (void *)     sizeof_grid_route_probe      );
  add_read_handler("sizeof_grid_route_reply"	     , ghi_read_handler, (void *)     sizeof_grid_route_reply      );
  
  add_read_handler("offsetof_grid_hdr_version"       , ghi_read_handler, (void *)     offsetof_grid_hdr_version    );
  add_read_handler("offsetof_grid_hdr_type"	     , ghi_read_handler, (void *)     offsetof_grid_hdr_type       );
  add_read_handler("offsetof_grid_hdr_ip"	     , ghi_read_handler, (void *)     offsetof_grid_hdr_ip         );
  add_read_handler("offsetof_grid_hdr_tx_ip"	     , ghi_read_handler, (void *)     offsetof_grid_hdr_tx_ip      );
  
  add_read_handler("offsetof_grid_nbr_encap_dst_ip" , ghi_read_handler, (void *)     offsetof_grid_nbr_encap_dst_ip);
  
  add_read_handler("offsetof_grid_loc_query_dst_ip" , ghi_read_handler, (void *)     offsetof_grid_loc_query_dst_ip);	
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridHeaderInfo)
