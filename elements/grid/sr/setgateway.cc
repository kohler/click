/*
 * SetGateway.{cc,hh} -- element tracks tcp flows sent to gateways
 * John Bicket
 * A lot of code ripped from lookupiprouteron.cc by Alexander Yip
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "setgateway.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
CLICK_DECLS

#ifndef setgateway_assert
#define setgateway_assert(e) ((e) ? (void) 0 : setgateway_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


SetGateway::SetGateway()
  :  Element(2,2),
     _gw_sel(0),
     _timer(this)
{
  MOD_INC_USE_COUNT;

}

SetGateway::~SetGateway()
{
  MOD_DEC_USE_COUNT;
}

int
SetGateway::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
		    cpElement, "GatewaySelector element", &_gw_sel,
                    cpKeywords,
                    0);

  if (!_gw_sel || _gw_sel->cast("GatewaySelector") == 0) 
    return errh->error("GatewaySelector element is not a GatewaySelector");

  return ret;
}

SetGateway *
SetGateway::clone () const
{
  return new SetGateway;
}

int
SetGateway::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
SetGateway::run_timer ()
{
}

void 
SetGateway::push_fwd(Packet *p_in, IPAddress best_gw) 
{
  const click_tcp *tcph = p_in->tcp_header();
  IPFlowID flowid = IPFlowID(p_in);
  FlowTableEntry *match = _flow_table.findp(flowid);

  if (!(tcph->th_flags & TH_SYN)) {
    if (match && !match->is_pending()) {
      match->saw_forward_packet();
      if (tcph->th_flags & (TH_RST | TH_FIN)) {
	match->_fwd_alive = false; // forward flow is over
      }
      if (tcph->th_flags & TH_RST) {
	match->_rev_alive = false; // rev flow is over
      }
      p_in->set_dst_ip_anno(match->_gw);
      output(0).push(p_in);
      return;
    }
    
    click_chatter("SetGateway %s: couldn't find non-pending match, killing\n",
		  id().cc());
    p_in->kill();
    return;
  }

  if (match && match->is_pending()) {
    match->_outstanding_syns++;
    p_in->set_dst_ip_anno(match->_gw);
    output(0).push(p_in);
    return;
  } 
  
  click_chatter("SetGateway %s: new flow for %s to %s\n",
		id().cc(),
		flowid.s().cc(),
		best_gw.s().cc());

  /* no match */
  _flow_table.insert(flowid, FlowTableEntry());
  match = _flow_table.findp(flowid);
  match->_id = flowid;
  match->_gw = best_gw;
  match->saw_forward_packet();
  match->_outstanding_syns++;
  p_in->set_dst_ip_anno(best_gw);
  output(0).push(p_in);
}


void 
SetGateway::push_rev(Packet *p_in) 
{
  const click_tcp *tcph = p_in->tcp_header();
  IPFlowID flowid = IPFlowID(p_in).rev();
  FlowTableEntry *match = _flow_table.findp(flowid);

  if ((tcph->th_flags & TH_SYN) && (tcph->th_flags & TH_ACK)) {
    if (match) {
      /* yes, using the dst_ip_anno is a bit weird to mark
       * the gw it came from, but there aren't any other
       * convenient ip annos. also in srcr.cc
       * --jbicket
       */
      if (match->_gw != MISC_IP_ANNO(p_in)) {
	click_chatter("SetGateway %s: got packet from weird gw %s, expected %s\n",
		      id().cc(),
		      p_in->dst_ip_anno().s().cc(),
		      match->_gw.s().cc());
	p_in->kill();
	return;
      }
      match->saw_reply_packet();
      match->_outstanding_syns = 0;
      output(1).push(p_in);
      return;
    }

    click_chatter("SetGateway %s: no match, killing SYN_ACK\n",
		  id().cc());
    p_in->kill();
    return;
  }
    

  /* not a syn-ack packet */
  if (match && !match->is_pending()) {
    match->saw_reply_packet();
    if (tcph->th_flags & (TH_FIN | TH_RST)) {
      match->_rev_alive = false;
    }
    if (tcph->th_flags & TH_RST) {
      match->_fwd_alive = false;
    }
    output(1).push(p_in);
    return;
  }

  click_chatter("SetGateway %s: couldn't find non-pending match, creating for %s\n",
		id().cc(),
		flowid.s().cc());

  _flow_table.insert(flowid, FlowTableEntry());
  match = _flow_table.findp(flowid);
  match->_id = flowid;
  match->_gw = MISC_IP_ANNO(p_in);
  match->saw_forward_packet();
  output(1).push(p_in);
  return;
}

void
SetGateway::push(int port, Packet *p_in)
{
  if (p_in->ip_header()->ip_p != IP_PROTO_TCP) {
    if (port == 0 && _gw_sel) {
      /* non tcp packets go to best gw */
      IPAddress gateway = _gw_sel->best_gateway();
      p_in->set_dst_ip_anno(gateway);
    } else {
      p_in->set_dst_ip_anno(IPAddress());
    }
    output(port).push(p_in);
    return;
  }


  const click_tcp *tcph;
  tcph = p_in->tcp_header();

  if (port == 0) {
    IPAddress gateway = _gw_sel->best_gateway();
    push_fwd(p_in, gateway);
  } else {
    /* incoming packet */
    push_rev(p_in);

  }
}


String
SetGateway::static_print_flows(Element *f, void *) 
{
  SetGateway *d = (SetGateway *) f;
  return d->print_flows();
}
String
SetGateway::print_flows()
{
  StringAccum sa;
  struct timeval now;
  click_gettimeofday(&now);
  for(FTIter iter = _flow_table.begin(); iter; iter++) {
    FlowTableEntry f = iter.value();
    sa << f._id << " gw " << f._gw << "\n";
  }

  return sa.take_string();

}
void
SetGateway::add_handlers()
{
  add_read_handler("flows", static_print_flows, 0);
}

void
SetGateway::setgateway_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("SetGateway %s assertion \"%s\" failed: file %s, line %d",
		id().cc(), expr, file, line);
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif

}

// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SetGateway::IPAddress>;
template class DEQueue<SetGateway::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SetGateway)
