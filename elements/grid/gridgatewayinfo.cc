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
#include <click/router.hh>
#include <click/error.hh>

GridGatewayInfo::GridGatewayInfo()
{
  MOD_INC_USE_COUNT;
  _is_gateway = false;
}

GridGatewayInfo::~GridGatewayInfo()
{
  MOD_DEC_USE_COUNT;
}

int
GridGatewayInfo::read_args(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpBool, "is this node a gateway?", &_is_gateway,
			0);
  return res;
}
int
GridGatewayInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return read_args(conf, errh);
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


static int
gw_write_handler(const String &arg, Element *element,
		 void *, ErrorHandler *errh)
{
  GridGatewayInfo *l = (GridGatewayInfo *) element;
  Vector<String> arg_list;
  cp_argvec(arg, arg_list);

  return l->read_args(arg_list, errh);
}

void
GridGatewayInfo::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("is_gateway", gw_read_handler, (void *) 0);
  add_write_handler("is_gateway", gw_write_handler, (void *) 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridGatewayInfo)
