/*
 * phyerrfilter.{cc,hh} -- filters packets out with phy errors
 * John Bicket
 *
 * Copyright (c) 2004 Massachussrcrs Institute of Technology
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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include "phyerrfilter.hh"

CLICK_DECLS


PhyErrFilter::PhyErrFilter()
  : Element(1, 1),
    _drops(0)
  
{
  MOD_INC_USE_COUNT;
}

PhyErrFilter::~PhyErrFilter()
{
  MOD_DEC_USE_COUNT;
}

void
PhyErrFilter::notify_noutputs(int n) 
{
  set_noutputs((n > 3 || n < 1) ? 1 : n);
}

int
PhyErrFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{

    if (cp_va_parse(conf, this, errh, 
		    cpKeywords,
		    cpEnd) < 0) {
      return -1;
    }
  return 0;
}

Packet *
PhyErrFilter::simple_action(Packet *p)
{

  int status = WIFI_RX_STATUS_ANNO(p);
  if (status == 0 && !WIFI_RX_ERR_ANNO(p)) {
    return p;
  } 

  if (noutputs() == 2) {
      output(1).push(p);
  } else {
    p->kill();
  }
  _drops++;
  return 0;
}

enum {H_DROPS };

static String
PhyErrFilter_read_param(Element *e, void *thunk)
{
  PhyErrFilter *td = (PhyErrFilter *)e;
  switch ((uintptr_t) thunk) {
  case H_DROPS: 
    return String(td->_drops) + "\n";
  default:
    return String();
  }

}
void
PhyErrFilter::add_handlers()
{
  add_read_handler("drops", PhyErrFilter_read_param, (void *) H_DROPS);
}

CLICK_ENDDECLS


EXPORT_ELEMENT(PhyErrFilter)



