/*
 * policyprobe.{cc,hh} -- Implements a policy for probing paths
 * Alexander Yip
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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
#include "lookupiprouteron.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/click_tcp.h>
#include <click/string.hh>
#include <click/straccum.hh>
#include <click/bitvector.hh>

#define dprintf if(0)printf

PolicyProbe::PolicyProbe() 
{
}

PolicyProbe::~PolicyProbe()
{
}

void 
PolicyProbe::handle_syn()
{
}

void
PolicyProbe::policy_handle_synack()
{
}

void PolicyProbe::push_forward_syn(Packet *p) 
{
}


void PolicyProbe::push_forward_fin(Packet *p) 
{
}

void PolicyProbe::push_forward_rst(Packet *p) 
{
}

void PolicyProbe::push_forward_normal(Packet *p) 
{

}

void PolicyProbe::push_forward_packet(Packet *p) 
{
}

void PolicyProbe::push_reverse_synack(unsigned inport, Packet *p) 
{

}
void PolicyProbe::push_reverse_fin(Packet *p) 
{
}
void PolicyProbe::push_reverse_rst(Packet *p) 
{
}
void PolicyProbe::push_reverse_normal(Packet *p) 
{
}

void PolicyProbe::push_reverse_packet(int inport, Packet *p) 
{
}

void PolicyProbe::expire_hook(Timer *, void *thunk) 
{
}




