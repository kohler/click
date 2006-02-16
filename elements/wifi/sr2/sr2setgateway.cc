/*
 * SR2SetGateway.{cc,hh} -- element tracks tcp flows sent to gateways
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
#include "sr2setgateway.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
CLICK_DECLS



SR2SetGateway::SR2SetGateway()
  :  _gw_sel(0),
     _timer(this)
{

}

SR2SetGateway::~SR2SetGateway()
{
}

int
SR2SetGateway::configure (Vector<String> &conf, ErrorHandler *errh)
{
  _gw = IPAddress();
  int ret;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "GW", cpIPAddress, "Gateway IP Address", &_gw,
		    "SEL", cpElement, "GatewaySelector element", &_gw_sel,
                    cpEnd);

  if (_gw_sel && _gw_sel->cast("SR2GatewaySelector") == 0) 
    return errh->error("SR2GatewaySelector element is not a SR2GatewaySelector");

  if (!_gw_sel && !_gw) {
    return errh->error("Either GW or SEL must be specified!\n");
  }
  return ret;
}

int
SR2SetGateway::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
SR2SetGateway::run_timer (Timer *)
{
  cleanup();
  _timer.schedule_after_msec(60*1000);
}

void 
SR2SetGateway::push_fwd(Packet *p_in, IPAddress best_gw) 
{
	const click_tcp *tcph = p_in->tcp_header();
	IPFlowID flowid = IPFlowID(p_in);
	FlowTableEntry *match = _flow_table.findp(flowid);
	
	
	if ((tcph->th_flags & TH_SYN) && match && match->is_pending()) {
		match->_outstanding_syns++;
		p_in->set_dst_ip_anno(match->_gw);
		output(0).push(p_in);
		return;
	}  else if (!(tcph->th_flags & TH_SYN)) {
		if (match) {
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
		
		click_chatter("%{element}::%s no match. guessing for %s\n",
			      this, __func__, flowid.s().c_str());
	}
	
	if (!best_gw) {
		p_in->kill();
		return;
	}

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
SR2SetGateway::push_rev(Packet *p_in) 
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
				click_chatter("%{element}::%s flow %s got packet from weird gw %s, expected %s\n",
					      this, __func__,
					      flowid.s().c_str(),
					      p_in->dst_ip_anno().s().c_str(),
					      match->_gw.s().c_str());
				p_in->kill();
				return;
			}
			match->saw_reply_packet();
			match->_outstanding_syns = 0;
			output(1).push(p_in);
			return;
		}
		
		click_chatter("SR2SetGateway %s: no match, killing SYN_ACK\n",
			      name().c_str());
		p_in->kill();
		return;
	}
	
	
	/* not a syn-ack packet */
	if (match) {
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
	
	click_chatter("%{element}::%s couldn't find non-pending match, creating %s\n",
		      this, __func__, flowid.s().c_str());
	
	_flow_table.insert(flowid, FlowTableEntry());
	match = _flow_table.findp(flowid);
	match->_id = flowid;
	match->_gw = MISC_IP_ANNO(p_in);
	match->saw_reply_packet();
	
	output(1).push(p_in);
	return;
}

void
SR2SetGateway::push(int port, Packet *p_in)
{
  if (_gw) {
    if (port == 0) {
      p_in->set_dst_ip_anno(_gw);
    } else {
      p_in->set_dst_ip_anno(IPAddress());
    }
    output(port).push(p_in);
    return;
  } else if (!_gw_sel) {
    /* this should never happen */
    click_chatter("%{element}: _gw and _gw_sel not specified! killing packet\n",
		  this);
    p_in->kill();
    return;
  }

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

void 
SR2SetGateway::cleanup() {
	FlowTable new_table;
	Timestamp timeout(60, 0);
	
	for(FTIter i = _flow_table.begin(); i; i++) {
		FlowTableEntry f = i.value();
		if (f.age() < timeout && f._fwd_alive || f._rev_alive) {
			new_table.insert(f._id, f);
		}
	}
	_flow_table.clear();
	for(FTIter i = new_table.begin(); i; i++) {
		FlowTableEntry f = i.value();
		_flow_table.insert(f._id, f);
	}
}
String
SR2SetGateway::static_print_flows(Element *f, void *) 
{
  SR2SetGateway *d = (SR2SetGateway *) f;
  return d->print_flows();
}
String
SR2SetGateway::print_flows()
{
  StringAccum sa;
  struct timeval now;
  click_gettimeofday(&now);
  for(FTIter iter = _flow_table.begin(); iter; iter++) {
    FlowTableEntry f = iter.value();
    sa << f._id << " gw " << f._gw << " age " << f.age() << "\n";
  }

  return sa.take_string();

}
String
SR2SetGateway::read_param(Element *e, void *vparam)
{
  SR2SetGateway *d = (SR2SetGateway *) e;
  switch ((int)vparam) {
  case 0:
    if (d->_gw) {
      return d->_gw.s() + "\n";
    }
    return "auto:" + d->_gw_sel->name() + "\n";;
  default:
    return "";
  }
}
int 
SR2SetGateway::change_param(const String &in_s, Element *e, void *thunk, ErrorHandler *errh)
{
  SR2SetGateway *d = static_cast<SR2SetGateway *>(e);
  int which = reinterpret_cast<int>(thunk);
  String s = cp_uncomment(in_s);
  switch (which) {
  case 0:
    {
      IPAddress ip;
      if (!cp_ip_address(s, &ip)) {
	return errh->error("gateway parameter must be IPAddress");
      }
      if (!ip && !d->_gw_sel) {
	return errh->error("gateway cannot be %s if _gw_sel is unspecified");
      }
      d->_gw = ip;
      return 0;
    }
  default:
    return errh->error("internal error");
  }

}
void
SR2SetGateway::add_handlers()
{
  add_read_handler("flows", static_print_flows, 0);

  add_read_handler("gateway", read_param, (void *) 0);
  add_write_handler("gateway", change_param, (void *) 0);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<SR2SetGateway::IPAddress>;
template class DEQueue<SR2SetGateway::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SR2SetGateway)
