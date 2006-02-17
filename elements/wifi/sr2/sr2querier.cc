/*
 * SR2Querier.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussrcrs Institute of Technology
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
#include "sr2querier.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "sr2linkmetric.hh"
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "sr2packet.hh"
#include "sr2forwarder.hh"
CLICK_DECLS



SR2Querier::SR2Querier()
  :  _en(),
     _sr_forwarder(0),
     _link_table(0)
{

  // Pick a starting sequence number that we have not used before.
  struct timeval tv;
  click_gettimeofday(&tv);
  _seq = tv.tv_usec;

  _query_wait = Timestamp(5, 0);

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

SR2Querier::~SR2Querier()
{
}

int
SR2Querier::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  _route_dampening = true;
  _time_before_switch_sec = 10;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "SR", cpElement, "SRForwarder element", &_sr_forwarder,
		    "LT", cpElement, "LinkTable element", &_link_table,
		    /* below not required */
		    "DEBUG", cpBool, "Debug", &_debug,
		    "ROUTE_DAMPENING", cpBool, "Enable Route Dampening", &_route_dampening,
		    "TIME_BEFORE_SWITCH", cpUnsigned, "", &_time_before_switch_sec,
                    cpEnd);

  if (!_en) 
    return errh->error("ETH not specified");
  if (!_link_table) 
    return errh->error("LT not specified");
  if (!_sr_forwarder) 
    return errh->error("SR not specified");


  if (_sr_forwarder->cast("SR2Forwarder") == 0) 
    return errh->error("SR2Forwarder element is not a SR2Forwarder");
  if (_link_table->cast("LinkTable") == 0) 
    return errh->error("LinkTable element is not a LinkTable");

  return ret;
}

int
SR2Querier::initialize (ErrorHandler *)
{
  return 0;
}


void
SR2Querier::send_query(IPAddress dst)
{

  DstInfo *nfo = _queries.findp(dst);
  if (!nfo) {
    _queries.insert(dst, DstInfo(dst));
    nfo = _queries.findp(dst);
  }
  nfo->_last_query.set_now();
  nfo->_count++;

  WritablePacket *p = Packet::make((unsigned)0);
  p->set_dst_ip_anno(dst);
  output(1).push(p);
}

void
SR2Querier::push(int, Packet *p_in)
{
	bool sent_packet = false;
	bool do_query = false;
	IPAddress dst = p_in->dst_ip_anno();
	
	if (!dst) {
		click_chatter("%{element}: got invalid dst %s\n",
			      this,
			      dst.s().c_str());
		p_in->kill();
		return;
	}
	
	DstInfo *q = _queries.findp(dst);
	if (!q) {
		DstInfo foo = DstInfo(dst);
		_queries.insert(dst, foo);
		q = _queries.findp(dst);
		q->_best_metric = 0;
		do_query = true;
	}
	
	Timestamp now = Timestamp::now();
	Timestamp expire = q->_last_switch + Timestamp(_time_before_switch_sec, 0);
	
	
	if (!q->_best_metric || !q->_p.size() || expire < now) {
		Path best = _link_table->best_route(dst, true);
		bool valid = _link_table->valid_route(best);
		q->_last_switch = now;
		if (valid) {
			if (q->_p != best) {
				q->_first_selected = now;
			}
			q->_p = best;
			q->_best_metric = _link_table->get_route_metric(best);
		} else {
			do_query = true;
			q->_p = Path();
			q->_best_metric = 0;
		}
	}
	
	if (q->_best_metric) {
		p_in = _sr_forwarder->encap(p_in, q->_p, 0);
		if (p_in) {
			output(0).push(p_in);
		}
		sent_packet = true;
	} else {
		/* no valid route, don't send. */
		click_chatter("%{element} :: %s no valid route to %s\n",
			      this,
			      __func__,
			      dst.s().c_str());
		p_in->kill();
	}
	
	if (do_query) {
		Timestamp expire = q->_last_query + _query_wait;
		if (expire < Timestamp::now()) {
			send_query(dst);
		}
	}
	return;
}

enum {H_DEBUG, H_PATH_CACHE, H_RESET, H_QUERIES, H_QUERY};

static String 
read_param(Element *e, void *thunk)
{
  SR2Querier *td = (SR2Querier *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    case H_QUERIES: {
        StringAccum sa;
	Timestamp now = Timestamp::now();
	for (SR2Querier::DstTable::const_iterator iter = td->_queries.begin(); iter; iter++) {
	  SR2Querier::DstInfo dst = iter.value();
	  sa << dst._ip;
	  sa << " query_count " << dst._count;
	  sa << " best_metric " << dst._best_metric;
	  sa << " last_query_ago " << now - dst._last_query;
	  sa << " first_selected_ago " << now - dst._first_selected;
	  sa << " last_switch_ago " << now - dst._last_switch;
	  int current_path_metric = td->_link_table->get_route_metric(dst._p);
	  sa << " current_path_metric " << current_path_metric;
	  sa << " [ ";
	  sa << path_to_string(dst._p);
	  sa << " ]";
	  Path best = td->_link_table->best_route(dst._ip, true);
	  int best_metric = td->_link_table->get_route_metric(best);
	  sa << " best_metric " << best_metric;
	  sa << " best_route [ ";
	  sa << path_to_string(best);
	  sa << " ]";
	  sa << "\n";
	}
	return sa.take_string();
    }
    default:
      return String();
    }
}
static int 
write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  SR2Querier *f = (SR2Querier *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_QUERY: {    //
    IPAddress dst;
    if (!cp_ip_address(s, &dst)) 
      return errh->error("query parameter must be IPAddress");
    f->send_query(dst);
    break;
  }
  case H_RESET:
    f->_queries.clear();
    break;
  }
  return 0;
}

void
SR2Querier::add_handlers()
{
  add_read_handler("queries", read_param, (void *) H_QUERIES);
  add_read_handler("debug", read_param, (void *) H_DEBUG);

  add_write_handler("debug", write_param, (void *) H_DEBUG);
  add_write_handler("reset", write_param, (void *) H_RESET);
  add_write_handler("query", write_param, (void *) H_QUERY);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SR2Querier::IPAddress>;
template class DEQueue<SR2Querier::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SR2Querier)
