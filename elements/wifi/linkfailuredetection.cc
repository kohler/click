/*
 * linkfailuredetection.{cc,hh} -- sets wifi txrate annotation on a packet
 * John Bicket
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <click/router.hh>
#include "linkfailuredetection.hh"
CLICK_DECLS

LinkFailureDetection::LinkFailureDetection()
  : Element(1, 1),
    _threshold(1)
{
  MOD_INC_USE_COUNT;
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

LinkFailureDetection::~LinkFailureDetection()
{
  MOD_DEC_USE_COUNT;
}

int
LinkFailureDetection::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (conf.size() != 2) {
    return errh->error("LinkFailureDetection need two args THRESHOLD and HANLDER");
  }
  
  if (!cp_integer(conf[0], &_threshold) || _threshold < 1) {
    return errh->error("THRESHOLD must be >= 1");
  }

  if (!cp_handler_name(conf[1], &_handler_e, &_handler_name, this, errh)) {
    return errh->error("invalid handler %s", conf[1].cc());
  }
  return 0;
}


void 
LinkFailureDetection::call_handler(EtherAddress dst) {
  ErrorHandler *errh = ErrorHandler::default_handler();
  
  const Handler *h = Router::handler(_handler_e, _handler_name);
  if (!h) {
    errh->error("%s: no handler `%s'", id().cc(), 
		Handler::unparse_name(_handler_e, _handler_name).cc());
  }
  
  if (h->writable()) {
    ContextErrorHandler cerrh
      (errh, "In write handler `" + h->unparse_name(_handler_e) + "':");
    h->call_write(dst.s(), _handler_e, &cerrh);
  } else {
    errh->error("%s: no write handler `%s'", 
		id().cc(), 
		h->unparse_name(_handler_e).cc());
  }

}
Packet *
LinkFailureDetection::simple_action(Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress dst = EtherAddress(eh->ether_dhost);

  if (dst == _bcast) {
    /* don't record bcast packets */
    return p_in;
  }
  click_wifi_extra *ceh = (click_wifi_extra *) p_in->all_user_anno();
  bool success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);

  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    DstInfo foo = DstInfo(dst);
    _neighbors.insert(dst, foo);
    nfo = _neighbors.findp(dst);
  }
  click_gettimeofday(&nfo->_last_received);
  if (success) {
    nfo->_successive_failures = 0;
    nfo->_notified = false;
  } else {
    nfo->_successive_failures++;
    StringAccum sa;
    sa  << nfo->_last_received;
    if (0 == nfo->_successive_failures % _threshold) {
      click_chatter("%{element}: succ. packet %d ethtype %x %s at %s\n",
		    this,
		    nfo->_successive_failures,
		    ntohs(eh->ether_type),
		    nfo->_eth.s().cc(),
		    sa.take_string().cc());


      /* call handler */
      call_handler(dst);
      nfo->_notified = true;
    }
  }
  return p_in;
}
String
LinkFailureDetection::static_print_stats(Element *e, void *)
{
  LinkFailureDetection *n = (LinkFailureDetection *) e;
  return n->print_stats();
}

String
LinkFailureDetection::print_stats() 
{
  struct timeval now;
  click_gettimeofday(&now);
  
  StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter; iter++) {
    DstInfo n = iter.value();
    struct timeval age = now - n._last_received;
    sa << n._eth.s().cc();
    sa << " successive_failures: " << n._successive_failures;
    if (n._notified) {
      sa << "*";
    }
    sa << " last_received: " << age << "\n";
  }
  return sa.take_string();
}
void
LinkFailureDetection::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("stats", static_print_stats, 0);

}
// generate Vector template instance
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, LinkFailureDetection::DstInfo>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(LinkFailureDetection)

