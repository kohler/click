/*
 * gridgatewayinfo.{cc,hh} -- specifies if node should advertise
 * itself as an internet gateway
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
#include "elements/grid/gridgatewayinfo.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/ipaddress.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

GridGatewayInfo::GridGatewayInfo()
  : Element(1,1)
{
  MOD_INC_USE_COUNT;
  _is_gateway = false;
}

GridGatewayInfo::~GridGatewayInfo()
{
  MOD_DEC_USE_COUNT;
}


int
GridGatewayInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpElement, "GridGenericRouteTable element", &_rt,
			cpBool, "is this node a gateway?", &_is_gateway,
			0);
  if (_rt == 0) 
    return errh->error("No route table specified");
  if (_rt->cast("GridGenericRouteTable") == 0)
    return errh->error("Route table element is not the right type");
  return res;

}

bool
GridGatewayInfo::is_gateway()
{
  return _is_gateway;
}

static String
gw_read_handler(Element *f, void *)
{
  GridGatewayInfo *l = (GridGatewayInfo *) f;
  
  const int BUFSZ = 256;
  char buf[BUFSZ];
  snprintf(buf, BUFSZ, "%s\n", l->is_gateway() ? "true" : "false");
  return String(buf);  
}

String
GridGatewayInfo::print_best_gateway(Element *f, void *)
{
  GridGatewayInfo *l = (GridGatewayInfo *) f;
  String s;
  
  GridGenericRouteTable::RouteEntry gw;
  if (l->_rt->current_gateway(gw)) {
    s += gw.dest_ip.s();
  } else {
    s += "none\n";
  }

  return s;
}

void
GridGatewayInfo::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("best_gateway", print_best_gateway, (void *) 0);
  add_read_handler("is_gateway", gw_read_handler, (void *) 0);

}

Packet *
GridGatewayInfo::simple_action(Packet *p) 
{
  GridGenericRouteTable::RouteEntry gw;
  if (_rt->current_gateway(gw)) {
    p->set_dst_ip_anno(gw.dest_ip);
    return p;
  } else {
    /* we couldn't find a gateway, so drop the packet */
    click_chatter("GridGatewayInfo: couldn't find a gateway. dropping packet");
    p->kill();
    return(0);
  }
  
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GridGatewayInfo)
