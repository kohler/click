// -*- c-basic-offset: 4 -*-
/*
 * ipsecroutetable.{cc,hh} -- looks up next-hop address in route table and includes support for IPsec ESP tunnels
 * between a pair of gateways (check push and cp_ipsec_route)
 * Dimitris Syrivelis
 *
 * Copyright (c) 2006 University of Thessaly, Hellas
 *
 * This is an extended version of iproutetable
 * iproutetable.{cc,hh} -- looks up next-hop address in route table
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 *
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, subject to the conditions listed in the Click LICENSE
 * file. These conditions include: you must preserve this copyright
 * notice, and you cannot mention the copyright holders in advertising
 * related to the Software without their permission.  The Software is
 * provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This notice is a
 * summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/packet_anno.hh>
#include "ipsecroutetable.hh"
#include "esp.hh"

CLICK_DECLS

//changed to support IPsec extensions
bool
cp_ipsec_route(String s, IPsecRoute *r_store, bool remove_route, Element *context)
{
    IPsecRoute r;
    //Data to initialize the SADataTuple
    unsigned int replay;
    uint8_t  oowin;

    SADataTuple * sa_data;

    if (!IPPrefixArg(true).parse(cp_shift_spacevec(s), r.addr, r.mask, context))
	return false;

    r.addr &= r.mask;
    String word = cp_shift_spacevec(s);
    if (word == "-")
	/* null gateway; do nothing */;
    else if (IPAddressArg().parse(word, r.gw, context))
	/* do nothing */;
    else
	goto two_words;

    word = cp_shift_spacevec(s);
  two_words:
    if (IntArg().parse(word, r.port) || (!word && remove_route))
	//Ipsec extensions parsing
	word = cp_shift_spacevec(s);

    if (!word) {
	//no further arguments found so no ipsec extensions need to be added for this route
	r.spi = SPI(0);
	r.sa_data = NULL;
	//store routing table
        *r_store = r;
	return true;
    }

    Vector<String> words;
    words.push_back(word);
    cp_spacevec(s, words);
    String enc_key, auth_key;
    if (Args(words, context, ErrorHandler::default_handler())
	.read_mp("SPI", r.spi)
	.read_mp("ENCRYPT_KEY", enc_key)
	.read_mp("AUTH_KEY", auth_key)
	.read_mp("REPLAY", replay)
	.read_mp("OOSIZE", oowin)
	.complete() < 0)
	return false;
    if (enc_key.length() != 16 || auth_key.length() != 16) {
	click_chatter("key has bad length");
	return false;
    }

    // Create new Security Association Table entry
    sa_data = new SADataTuple(enc_key.data(), auth_key.data(), replay, oowin);
    ((IPsecRouteTable*)context)->_sa_table.insert(SPI(r.spi),*sa_data);
    //Set Tuple reference in the Routing entry
    r.sa_data = sa_data;
    //store routing table
    *r_store = r;
    return true;
}

//changed to support ipsec extensions
StringAccum&
IPsecRoute::unparse(StringAccum& sa, bool tabs) const
{
    int l = sa.length();
    char tab = (tabs ? '\t' : ' ');
    sa << addr.unparse_with_mask(mask) << tab;
    if (sa.length() < l + 17 && tabs)
	sa << '\t';
    l = sa.length();
    if (gw)
	sa << gw << tab;
    else
	sa << '-' << tab;
    if (sa.length() < l + 9 && tabs)
	sa << '\t';
    if (!real())
	sa << "-1";
    else
	sa << port;
    if(spi != 0) {
	sa << "  |TUNNELED CONNECTION| \n|SPI| |ENC KEY| |AUTH KEY| ||\n";
	sa << " |" <<spi<<"|";
	sa << sa_data->unparse_entries().c_str() <<"\n";
    }
    return sa;
}

String
IPsecRoute::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}


void *
IPsecRouteTable::cast(const char *name)
{
    if (strcmp(name, "IPsecRouteTable") == 0)
	return (void *)this;
    else
	return Element::cast(name);
}

int
IPsecRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
    IPsecRoute r;
    for (int i = 0; i < conf.size(); i++) {
	if (cp_ipsec_route(conf[i], &r, false, this)
	    && r.port >= 0 && r.port < noutputs())
	    (void) add_route(r, false, 0, errh);
	else
	    errh->error("argument %d should be 'ADDR/MASK [GATEWAY] OUTPUT'", i+1);
    }
    return errh->nerrors() ? -1 : 0;
}

int
IPsecRouteTable::add_route(const IPsecRoute&, bool, IPsecRoute*, ErrorHandler *errh)
{
    // by default, cannot add routes
    return errh->error("cannot add routes to this routing table");
}

int
IPsecRouteTable::remove_route(const IPsecRoute&, IPsecRoute*, ErrorHandler *errh)
{
    // by default, cannot remove routes
    return errh->error("cannot delete routes from this routing table");
}

int
IPsecRouteTable::lookup_route(IPAddress, IPAddress &, unsigned int&, SADataTuple*&) const
{
    return -1;			// by default, route lookups fail
}

String
IPsecRouteTable::dump_routes()
{
    return String();
}

void
IPsecRouteTable::push(int, Packet *p)
{

    IPAddress gw;
    uint32_t spi;
    SADataTuple * sa_data;
    const click_ip *ip = reinterpret_cast< const click_ip *>(p->data());

    int port = lookup_route(p->dst_ip_anno(), gw, spi, sa_data);

    if (port >= 0) {
	switch(port) {
	  case 1: {
	   //This packet should be sent over a tunneled connection
	   //so set proper annotations with references to Security Data to be used by IPsec modules
           if((spi == 0) || (sa_data == NULL)) {
	       click_chatter("No Ipsec tunnel for %s. Wrong tunnel setup", p->dst_ip_anno().unparse().c_str());
	   }
	   SET_IPSEC_SPI_ANNO(p,(uint32_t)spi);
	   //ISSUE: This is 32-bit architecture specific passing a pointer to next module through annotations!!
	   SET_IPSEC_SA_DATA_REFERENCE_ANNO(p, (uintptr_t)sa_data);
	   break;
	 }
	 case 0: {
	    //Is this packet an ipsec ESP packet? What if one just needs to communicate with a server
            //that runs on this router?
	    if(ip->ip_p != 50) {
	      /*This not an IPSEC packet and it should be delivered to the host's linux network stack
                In a typical setup one would send anything that is directed to port 2 to Linux */
                port = 2;
            }
            // This is an ipsec packet and belongs to a tunneled connection
	    // so we set the proper annotation with reference to Security Data Table to be used by IPsec modules
            // Careful this enhancement is 32-bit architecture specific!!
            struct esp_new * esp =(struct esp_new *)(p->data()+sizeof(click_ip));
            sa_data = _sa_table.lookup(SPI(ntohl(esp->esp_spi)));
	    if(sa_data == NULL) {
		click_chatter("Invalid SPI %d, Dropping packet",ntohl(esp->esp_spi));
	p->kill();
                return;
           }
	   SET_IPSEC_SA_DATA_REFERENCE_ANNO(p, (uintptr_t)sa_data);
	   break;
	 }
	}; //end of switch
	assert(port < noutputs());
	if (gw)
	    p->set_dst_ip_anno(gw);
	output(port).push(p);
    } else {
	static int complained = 0;
	if (++complained <= 5)
	    click_chatter("IPsecRouteTable: no route for %s", p->dst_ip_anno().unparse().c_str());
	p->kill();
    }
}


int
IPsecRouteTable::run_command(int command, const String &str, Vector<IPsecRoute> * old_routes, ErrorHandler *errh)
{
    IPsecRoute route, old_route;
    if (!cp_ipsec_route(str, &route, command == CMD_REMOVE, this)
	|| route.port < (command == CMD_REMOVE ? -1 : 0)
	|| route.port >= noutputs())
	return errh->error("expected 'ADDR/MASK [GATEWAY%s'", (command == CMD_REMOVE ? " OUTPUT]" : "] OUTPUT"));

    int r, before = errh->nerrors();
    if (command == CMD_ADD)
	r = add_route(route, false, &old_route, errh);
    else if (command == CMD_SET)
	r = add_route(route, true, &old_route, errh);
    else
	r = remove_route(route, &old_route, errh);

    // save old route if in a transaction
    if (r >= 0 && old_routes) {
	if (old_route.port < 0) { // must come from add_route
	    old_route = route;
	    old_route.extra = CMD_ADD;
	} else
	    old_route.extra = command;
	old_routes->push_back(old_route);
    }

    // report common errors
    if (r == -EEXIST && errh->nerrors() == before)
	errh->error("conflict with existing route '%s'", old_route.unparse().c_str());
    if (r == -ENOENT && errh->nerrors() == before)
	errh->error("route '%s' not found", route.unparse().c_str());
    return r;
}


int
IPsecRouteTable::add_route_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
    IPsecRouteTable *table = static_cast<IPsecRouteTable *>(e);
    return table->run_command((thunk ? CMD_SET : CMD_ADD), conf, 0, errh);
}

int
IPsecRouteTable::remove_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    IPsecRouteTable *table = static_cast<IPsecRouteTable *>(e);
    return table->run_command(CMD_REMOVE, conf, 0, errh);
}

int
IPsecRouteTable::ctrl_handler(const String &conf_in, Element *e, void *, ErrorHandler *errh)
{
    IPsecRouteTable *table = static_cast<IPsecRouteTable *>(e);
    String conf = cp_uncomment(conf_in);
    const char* s = conf.begin(), *end = conf.end();

    Vector<IPsecRoute> old_routes;
    int r = 0;

    while (s < end) {
	const char* nl = find(s, end, '\n');
	String line = conf.substring(s, nl);

	String first_word = cp_shift_spacevec(line);
	int command;
	if (first_word == "add")
	    command = CMD_ADD;
	else if (first_word == "remove")
	    command = CMD_REMOVE;
	else if (first_word == "set")
	    command = CMD_SET;
	else if (!first_word)
	    continue;
	else {
	    r = errh->error("bad command '%#s'", first_word.c_str());
	    goto rollback;
	}

	if ((r = table->run_command(command, line, &old_routes, errh)) < 0)
	    goto rollback;

	s = nl + 1;
    }
    return 0;

  rollback:
    while (old_routes.size()) {
	const IPsecRoute& rt = old_routes.back();
	if (rt.extra == CMD_REMOVE)
	    table->add_route(rt, false, 0, errh);
	else if (rt.extra == CMD_ADD)
	    table->remove_route(rt, 0, errh);
	else
	    table->add_route(rt, true, 0, errh);
	old_routes.pop_back();
    }
    return r;
}

String
IPsecRouteTable::table_handler(Element *e, void *)
{
    IPsecRouteTable *r = static_cast<IPsecRouteTable*>(e);
    return r->dump_routes();
}

int
IPsecRouteTable::lookup_handler(int, String& s, Element* e, const Handler*, ErrorHandler* errh)
{
    IPsecRouteTable *table = static_cast<IPsecRouteTable*>(e);
    IPAddress a;
    if (IPAddressArg().parse(cp_uncomment(s), a, table)) {
	IPAddress gw;
	uint32_t spi;
	SADataTuple * sa_data;
	int port = table->lookup_route(a, gw,spi,sa_data);
	if (gw)
	    s = String(port) + " " + gw.unparse();
	else
	    s = String(port);
	return 0;
    } else
	return errh->error("expected IP address, not '%s'", s.c_str());
}

void
IPsecRouteTable::add_handlers()
{
    add_write_handler("add", add_route_handler, 0);
    add_write_handler("set", add_route_handler, 1);
    add_write_handler("remove", remove_route_handler, 0);
    add_write_handler("ctrl", ctrl_handler, 0);
    add_read_handler("table", table_handler, 0);
    set_handler("lookup", Handler::OP_READ | Handler::READ_PARAM, lookup_handler);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(IPsecRouteTable)
