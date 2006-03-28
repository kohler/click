/*
 * dhcpserver.{cc,hh} -- track dhcp leases from a free pool
 * John Bicket
 *
 * Copyright (c) 2006 Massachusetts Institute of Technology
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
#include <click/etheraddress.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <click/bighashmap.cc>
#include <click/vector.cc>
#include <click/straccum.hh>
#include <click/atomic.hh>
#include <click/packet_anno.hh>
#include <clicknet/dhcp.h>
#include <clicknet/ether.h>
#include "dhcpserver.hh"

DHCPServer::DHCPServer()
{
	_bcast = IPAddress(~0);
}

DHCPServer::~DHCPServer()
{
}

void *
DHCPServer::cast(const char *n) 
{
	if (strcmp(n, "DHCPServer") == 0)
		return (DHCPServer *)this;
	return 0;
}

int
DHCPServer::configure( Vector<String> &conf, ErrorHandler *errh )
{
	_debug = false;
	if (cp_va_parse(conf, this, errh,
			cpIPAddress, "server ip address", &_ip,
			cpKeywords, 
			"DEBUG", cpBool, "debug", &_debug,
			"START", cpIPAddress, "start", &_start,
			"END", cpIPAddress, "end", &_end,
			"BCAST", cpIPAddress, "bcast", &_bcast,
			cpEnd) < 0) {
		return -1;
	}
	for (u_int32_t x = ntohl(_start.addr()); x < ntohl(_end.addr()); x++) {
		IPAddress ip = IPAddress(htonl(x));
		//click_chatter("%s: inserting ip %s\n", __func__, ip.s().c_str());
		_free.insert(ip, ip);
		_free_list.push_back(ip);
	}
	return 0;
}

unsigned char* 
get_dhcp_option(unsigned char *options, int option_val, int *option_size)
{
	unsigned char *curr_ptr = options;
	/* XXX this loop should be limited... */
	while(curr_ptr[0] != DHO_END) {
		if(curr_ptr[0] == option_val) {
			*option_size = *(curr_ptr + 1);
			return curr_ptr+2;
		}
		uint32_t size = *(curr_ptr + 1);
		curr_ptr += (size + 2);
	}
	return NULL;
}

Lease *
DHCPServer::lookup(IPAddress ip)
{
	return _leases.findp(ip);
}
Lease *
DHCPServer::rev_lookup(EtherAddress eth)
{
	IPAddress *ip = _ips.findp(eth);
	return ip ? _leases.findp(*ip) : 0;
}
Lease *
DHCPServer::new_lease(EtherAddress eth, IPAddress ip)
{
	Lease *l = rev_lookup(eth);
	if (l) {
		return l;
	}
	if (_free.findp(ip)) {
		Lease l;
		l._eth = eth;
		l._ip = ip;
		l._start = Timestamp::now();
		l._end = l._start + Timestamp(60, 0);
		l._duration = l._end - l._start;
		insert(l);
		return lookup(ip);
	}
	return 0;
}
Lease *
DHCPServer::new_lease_any(EtherAddress eth)
{
	Lease *l = rev_lookup(eth);
	if (l) {
		return l;
	}
	while (_free_list.size()) {
		IPAddress next = _free_list[0];
		_free_list.pop_front();
		if (_free.findp(next)) {
			return new_lease(eth, next);
		}
	}
	return 0;
}

void
DHCPServer::insert(Lease l) {
	_free.remove(l._ip);
	_ips.insert(l._eth, l._ip);
	_leases.insert(l._ip, l);
}
void
DHCPServer::remove(EtherAddress eth)
{
	Lease *l = rev_lookup(eth);
	if (l) {
		_free.insert(l->_ip, l->_ip);
		_free_list.push_back(l->_ip);
		
		_leases.remove(l->_ip);
		_ips.remove(l->_eth);
	}
}
void
DHCPServer::push(int port, Packet *p_in)
{
	click_dhcp *msg = (click_dhcp *) (p_in->data() + sizeof(click_ip) + sizeof(click_udp));
	unsigned char *buf = NULL;
	int size = 0;
	IPAddress ciaddr = IPAddress(msg->ciaddr);
	IPAddress requested_ip = IPAddress(0);
	int optionFieldSize;
	unsigned char *m = get_dhcp_option(msg->options, DHO_DHCP_MESSAGE_TYPE, &optionFieldSize);
	int msg_type = (int) *m;
	EtherAddress eth = EtherAddress(msg->chaddr);
	Packet *out = 0;
	switch (msg_type) {
	case DHCP_REQUEST: {
		if (_debug) {
			click_chatter("%{element} request\n", this);
		}
		IPAddress server = IPAddress(0);
		Lease *lease = rev_lookup(eth);
		buf = get_dhcp_option(msg->options, 
				DHO_DHCP_SERVER_IDENTIFIER, &size);
		if (buf != NULL) {
			uint32_t server_id;
			memcpy(&server_id, buf, size);
			server = IPAddress(server_id);
		}
		
		buf = get_dhcp_option(msg->options, DHO_DHCP_REQUESTED_ADDRESS, &size );
		if (buf != NULL) {
			uint32_t requested_ip;
			memcpy( &requested_ip, buf, size );
			requested_ip = IPAddress(requested_ip);
		}
		
		if (!ciaddr && !requested_ip) {
			/* this is outside of the spec, but dhclient seems to
			   do this, so just give it an address */
			if (!lease) {
				lease = new_lease_any(eth);
			}
			if (lease) {
				out = make_ack_packet(p_in, lease);
			}
		} else if (server && !ciaddr && requested_ip) {
			/* SELECTING */
			if(lease && lease->_ip == requested_ip) {
				out = make_ack_packet(p_in, lease);
				lease->_valid = true;
			}
		} else if (!server && requested_ip && !ciaddr) {
			/* INIT-REBOOT */
			bool network_is_correct = true;
			if (!network_is_correct) {
				out = make_nak_packet(p_in, lease);
			} else {	  
				if (lease && lease->_ip == requested_ip) {
					if (lease->_end <  Timestamp::now() ) {
						out = make_nak_packet(p_in, lease);
					} else {
						lease->_valid = true;
						out = make_ack_packet(p_in, lease);
					}
				}
			}
		} else if (!server && !requested_ip && ciaddr) {
			/* RENEW or REBIND */
			if (lease) {
				lease->_valid = true;
				lease->extend();
				out = make_ack_packet(p_in, lease);
			}
		} else {
			click_chatter("%s:%d\n", __FILE__, __LINE__);
		}
		
		if (out) {
			goto done;
		}	
		break;
	}
	case DHCP_DISCOVER: {
		if (_debug) {
			click_chatter("%{element} discover\n", this);
		}
		Lease *l = 0;
		unsigned char *buf = get_dhcp_option(msg->options, 
					       DHO_DHCP_REQUESTED_ADDRESS, 
					       &optionFieldSize);
		if (buf) {
			IPAddress client_request_ip = IPAddress(buf);
			l = new_lease(eth, client_request_ip);
		}
		if (!l) {
			l = new_lease_any(eth);
		}
		if (l) {
			out = make_offer_packet(p_in, l);
			goto done;
		}
		break;
	}
	case DHCP_RELEASE: {
		if (_debug) {
			click_chatter("%{element} release\n", this);
		}
		buf = get_dhcp_option(msg->options, 
				DHO_DHCP_SERVER_IDENTIFIER, 
				&optionFieldSize);
		IPAddress incoming_server_id(buf);
		if (incoming_server_id == _ip) {
			remove(eth);
		}
		break;
	}
	default:
		break;
	}

 done:
	if (out) {
		/* push the ip and udp headers */
		WritablePacket *p_out = out->push(sizeof(click_udp) + sizeof(click_ip));
		if (p_out) {
			click_ip *ip = reinterpret_cast<click_ip *>(p_out->data());
			click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

			// set up IP header
			ip->ip_v = 4;
			ip->ip_hl = sizeof(click_ip) >> 2;
			ip->ip_len = htons(p_out->length());
			static atomic_uint32_t id = 0;
			ip->ip_id = htons(id.fetch_and_add(1));
			ip->ip_p = IP_PROTO_UDP;
			ip->ip_src = _ip;
			ip->ip_dst = _bcast;
			ip->ip_tos = 0;
			ip->ip_off = 0;
			ip->ip_ttl = 250;
			
			ip->ip_sum = 0;
			ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
			p_out->set_dst_ip_anno(_bcast);
			p_out->set_ip_header(ip, sizeof(click_ip));
			
			// set up UDP header
			udp->uh_sport = htons(67);
			udp->uh_dport = htons(68);
			uint16_t len = p_out->length() - sizeof(click_ip);
			udp->uh_ulen = htons(len);
			udp->uh_sum = 0;
			unsigned csum = click_in_cksum((unsigned char *)udp, len);
			udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, len);

			/* XXX node the query came from */
			IPAddress node = MISC_IP_ANNO(p_in);				
			p_out->set_dst_ip_anno(node);
			
			output(port).push(p_out);
		}
	}
	p_in->kill();
	return;
}

Packet*
DHCPServer::make_ack_packet(Packet *p, Lease *lease)
{
	if (_debug) {
		click_chatter("%{element}:%s\n", this, __func__);
	}
	click_dhcp *req_msg 
		= (click_dhcp*)(p->data() + sizeof(click_udp) + sizeof(click_ip));
	WritablePacket *ack_q = Packet::make(sizeof(click_dhcp));
	memset(ack_q->data(), '\0', ack_q->length());
	click_dhcp *dhcp_ack =
		reinterpret_cast<click_dhcp *>(ack_q->data());
	uint8_t *options_ptr;
	
	dhcp_ack->op = DHCP_BOOTREPLY;
	dhcp_ack->htype = ETH_10MB;
	dhcp_ack->hlen = ETH_10MB_LEN;
	dhcp_ack->hops = 0;
	dhcp_ack->xid = req_msg->xid; 
	dhcp_ack->secs = 0;
	dhcp_ack->flags = 0;
	dhcp_ack->ciaddr = req_msg->ciaddr;
	dhcp_ack->yiaddr = lease->_ip;
	dhcp_ack->siaddr = 0;
	dhcp_ack->flags = req_msg->flags;
	dhcp_ack->giaddr = req_msg->giaddr;
	memcpy(dhcp_ack->chaddr, req_msg->chaddr, 16);
	dhcp_ack->magic = DHCP_MAGIC;  
	options_ptr = dhcp_ack->options;
	*options_ptr++ = DHO_DHCP_MESSAGE_TYPE;
	*options_ptr++ = 1;
	*options_ptr++ = DHCP_ACK;
	*options_ptr++ = DHO_DHCP_LEASE_TIME;
	*options_ptr++ = 4;
	uint32_t duration = lease->_duration.sec(); 
	duration = htonl(duration);
	memcpy(options_ptr, &duration, 4);
	options_ptr += 4;
	*options_ptr++ = DHO_DHCP_SERVER_IDENTIFIER;
	*options_ptr++ = 4;
	memcpy(options_ptr, &_ip, 4);
	options_ptr += 4;
	*options_ptr = DHO_END;
	
	return ack_q;
}

Packet*
DHCPServer::make_nak_packet(Packet *p, Lease *)
{
	if (_debug) {
		click_chatter("%{element}:%s\n", this, __func__);
	}
	click_dhcp *req_msg =
		(click_dhcp*)(p->data() + sizeof(click_udp) + sizeof(click_ip));
	
	WritablePacket *nak_q = Packet::make(sizeof(click_dhcp));
	memset(nak_q->data(), '\0', nak_q->length());
	click_dhcp *dhcp_nak =
		reinterpret_cast<click_dhcp *>(nak_q->data());
	uint8_t *options_ptr;
	
	dhcp_nak->op = DHCP_BOOTREPLY;
	dhcp_nak->htype = ETH_10MB;
	dhcp_nak->hlen = ETH_10MB_LEN;
	dhcp_nak->hops = 0;
	dhcp_nak->xid = req_msg->xid;
	dhcp_nak->secs = 0;
	dhcp_nak->flags = 0;
	dhcp_nak->ciaddr = 0;
	dhcp_nak->yiaddr = 0;
	dhcp_nak->siaddr = 0;
	dhcp_nak->flags = req_msg->flags;
	dhcp_nak->giaddr = req_msg->giaddr;
	memcpy(dhcp_nak->chaddr, req_msg->chaddr, 16);
	dhcp_nak->magic = DHCP_MAGIC;
	options_ptr = dhcp_nak->options;
	*options_ptr++ = DHO_DHCP_MESSAGE_TYPE;
	*options_ptr++ = 1;
	*options_ptr++ = DHCP_NACK;
	*options_ptr++ = DHO_DHCP_SERVER_IDENTIFIER;
	*options_ptr++ = 4;
	memcpy(options_ptr, &_ip, 4);
	options_ptr += 4;
	*options_ptr = DHO_END;
	
	return nak_q;
}
Packet*
DHCPServer::make_offer_packet(Packet *p, Lease *l)
{
	if (_debug) {
		click_chatter("%{element}::%s\n", this, __func__);
	}
	WritablePacket *offer_q = Packet::make(sizeof(click_dhcp));
	memset(offer_q->data(), '\0', offer_q->length());
	click_dhcp *msg = (click_dhcp*)(p->data() + sizeof(click_udp) + sizeof(click_ip));
	click_dhcp *dhcp_offer = 
		reinterpret_cast<click_dhcp *>(offer_q->data());
	uint8_t *option_ptr;
	
	dhcp_offer->op = DHCP_BOOTREPLY;
	dhcp_offer->htype = ETH_10MB;
	dhcp_offer->hlen = ETH_10MB_LEN;
	dhcp_offer->hops = 0;
	dhcp_offer->xid = msg->xid; 
	dhcp_offer->secs = 0;
	dhcp_offer->flags = 0;
	dhcp_offer->ciaddr = 0;
	dhcp_offer->yiaddr = l->_ip;
	dhcp_offer->siaddr = 0;
	dhcp_offer->giaddr = 0;
	memcpy(dhcp_offer->chaddr, msg->chaddr, 16);
	dhcp_offer->magic = DHCP_MAGIC;
	//option field
	option_ptr = dhcp_offer->options;
	*option_ptr++ = DHO_DHCP_MESSAGE_TYPE;
	*option_ptr++ = 1;
	*option_ptr++ = DHCP_OFFER;
	
	*option_ptr++ = DHO_DHCP_LEASE_TIME;
	uint32_t duration = l->_duration.sec();
	*option_ptr++ = 4;
	memcpy(option_ptr, &duration, 4);
	option_ptr += 4;
	
	*option_ptr++ = DHO_DHCP_SERVER_IDENTIFIER;
	*option_ptr++ = 4;
	memcpy(option_ptr, &_ip, 4);
	option_ptr += 4;
	
	*option_ptr = DHO_END;
	
	return offer_q;
}

enum {H_LEASES};

String
DHCPServer::read_handler(Element *e, void *thunk)
{
	DHCPServer *lt = (DHCPServer *)e;
	switch ((uintptr_t) thunk) {
	case H_LEASES: {
		StringAccum sa;
		for (LeaseIter iter = lt->_leases.begin(); iter; iter++) {
			Lease l = iter.value();
			sa << "lease " << l._ip << " {\n";
			sa << "  starts " << l._start.sec() << ";\n";
			sa << "  ends " << l._end.sec() << ";\n";
			sa << "  hardware ethernet " << l._eth << ";\n";
			sa << "}\n";
		}
		return sa.take_string() + "\n";
	}
	default:
		return String();
	}
}

void 
DHCPServer::add_handlers()
{
	add_read_handler("leases", DHCPServer::read_handler, (void *) H_LEASES);
}

EXPORT_ELEMENT(DHCPServer)
#include <click/dequeue.cc>
#include <click/vector.cc>
template class DEQueue<IPAddress>;
