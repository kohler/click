/*
 * filterphyerr.{cc,hh} -- filters packets out with phy errors
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
#include <clicknet/wifi.h>
#include "filterphyerr.hh"

CLICK_DECLS


FilterPhyErr::FilterPhyErr()
  : Element(1, 1),
    _drops(0)
  
{
  MOD_INC_USE_COUNT;
}

FilterPhyErr::~FilterPhyErr()
{
  MOD_DEC_USE_COUNT;
}

void
FilterPhyErr::notify_noutputs(int n) 
{
  set_noutputs((n > 3 || n < 1) ? 1 : n);
}

int
FilterPhyErr::configure(Vector<String> &conf, ErrorHandler *errh)
{

    if (cp_va_parse(conf, this, errh, 
		    cpKeywords,
		    cpEnd) < 0) {
      return -1;
    }
  return 0;
}

Packet *
FilterPhyErr::simple_action(Packet *p)
{
  struct click_wifi_extra *ceha = (struct click_wifi_extra *) p->all_user_anno();  
  struct click_wifi_extra *cehp = (struct click_wifi_extra *) p->data();
  
  
  if ((ceha->magic == WIFI_EXTRA_MAGIC && ceha->flags & WIFI_EXTRA_RX_ERR) ||
      (cehp->magic == WIFI_EXTRA_MAGIC && cehp->flags & WIFI_EXTRA_RX_ERR)) {
    if (noutputs() == 2) {
      output(1).push(p);
    } else {
      p->kill();
    }
    _drops++;
    return 0;
  }
  return p;
}

enum {H_DROPS };

static String
FilterPhyErr_read_param(Element *e, void *thunk)
{
  FilterPhyErr *td = (FilterPhyErr *)e;
  switch ((uintptr_t) thunk) {
  case H_DROPS: 
    return String(td->_drops) + "\n";
  default:
    return String();
  }

}
void
FilterPhyErr::add_handlers()
{
  add_read_handler("drops", FilterPhyErr_read_param, (void *) H_DROPS);
}

CLICK_ENDDECLS


EXPORT_ELEMENT(FilterPhyErr)



