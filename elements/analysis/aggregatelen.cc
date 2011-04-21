// -*- mode: c++; c-basic-offset: 4 -*-

#include <click/config.h>
#include "aggregatelen.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
#include <click/packet_anno.hh>
CLICK_DECLS

AggregateLength::AggregateLength()
{
}

AggregateLength::~AggregateLength()
{
}

int
AggregateLength::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _ip = false;
    if (Args(conf, this, errh)
	.read("IP", _ip)
	.complete() < 0)
	return -1;
    return 0;
}

Packet *
AggregateLength::bad_packet(Packet *p)
{
    if (noutputs() == 2)
	output(1).push(p);
    else
	p->kill();
    return 0;
}

Packet *
AggregateLength::handle_packet(Packet *p)
{
    int offset;
    if (_ip && !p->has_network_header())
	return bad_packet(p);
    else if (_ip)
	offset = p->network_header_offset();
    else
	offset = 0;

    uint32_t len = p->length() - offset + EXTRA_LENGTH_ANNO(p);
    SET_AGGREGATE_ANNO(p, len);

    return p;
}

void
AggregateLength::push(int, Packet *p)
{
    if (Packet *q = handle_packet(p))
	output(0).push(q);
}

Packet *
AggregateLength::pull(int)
{
    Packet *p = input(0).pull();
    if (p)
	p = handle_packet(p);
    return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AggregateLength)
