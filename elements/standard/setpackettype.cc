// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * setpackettype.{cc,hh} -- element sets packet type annotation
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include "setpackettype.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

SetPacketType::SetPacketType()
{
}

int
SetPacketType::parse_type(const String &s)
{
    if (s == "HOST")
	return Packet::HOST;
    else if (s == "BROADCAST")
	return Packet::BROADCAST;
    else if (s == "MULTICAST")
	return Packet::MULTICAST;
    else if (s == "OTHERHOST")
	return Packet::OTHERHOST;
    else if (s == "OUTGOING")
	return Packet::OUTGOING;
    else if (s == "LOOPBACK")
	return Packet::LOOPBACK;
    else
	return -1;
}

const char *
SetPacketType::unparse_type(int p)
{
    switch (p) {
      case Packet::HOST:	return "HOST";
      case Packet::BROADCAST:	return "BROADCAST";
      case Packet::MULTICAST:	return "MULTICAST";
      case Packet::OTHERHOST:	return "OTHERHOST";
      case Packet::OUTGOING:	return "OUTGOING";
      case Packet::LOOPBACK:	return "LOOPBACK";
      default:			return "??";
    }
}

int
SetPacketType::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String ptype;
    if (Args(conf, this, errh).read_mp("TYPE", WordArg(), ptype).complete() < 0)
	return -1;
    int ptype_val = parse_type(ptype.upper());
    if (ptype_val < 0)
	return errh->error("unrecognized packet type %<%s%>", ptype.c_str());
    _ptype = (Packet::PacketType)ptype_val;
    return 0;
}

Packet *
SetPacketType::simple_action(Packet *p)
{
    p->set_packet_type_anno(_ptype);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetPacketType)
ELEMENT_MT_SAFE(SetPacketType)
