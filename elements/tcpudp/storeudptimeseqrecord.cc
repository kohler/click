/*
Programmer: Roman Chertov
 * Copyright (c) 2010 The Aerospace Corporation
 
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
#include "storeudptimeseqrecord.hh"
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <stdio.h>



CLICK_DECLS

/*
 */


StoreUDPTimeSeqRecord::StoreUDPTimeSeqRecord()
{
    _count = 0;
    _delta = 0;
    _offset = 0;
}



int StoreUDPTimeSeqRecord::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                    "OFFSET", cpkP+cpkM, cpInteger, &_offset,
                    "DELTA", 0, cpBool, &_delta,
                    cpEnd) < 0)
        return -1;
    return 0;
}

// This is the tricky bit.  We can't rely on any headers being
// filled out if elements like CheckIPHeader have not been called
// so we should just access the raw data and cast wisely
Packet* StoreUDPTimeSeqRecord::simple_action(Packet *packet)
{
    WritablePacket *p = packet->uniqueify();
    Timestamp       tnow;
    click_udp      *udph = 0;
    PData          *pData;
    uint32_t        csum = 0;
    uint32_t        offset = _offset;

    // the packet is shared and we can't modify it without screwing
    // things up
    if (!p)
    {
        // If uniqueify() fails, packet itself is garbage and has been deleted
        click_chatter("Non-Writable Packet!");
        return 0;
    }
    if (p->length() < offset + sizeof(uint32_t) * 2) // get the first two words of the IP header to see 
    {                                                // what it is to determine how to proceed further
        p->kill();
        return 0;
    }
    // here need to get to the right offset.  The IP header can be of variable length due to options
    // also, the header might be IPv4 of IPv6
    u_char version = p->data()[offset] >> 4;
    if (version == 0x04)
    {
        const click_ip *ip = reinterpret_cast<const click_ip *>(p->data() + offset);
        offset += ip->ip_hl << 2;
    }
    else if (version == 0x06)
    {
        // IPv6 header is 
        const click_ip6 *ip6 = reinterpret_cast<const click_ip6 *>(p->data() + offset);
        if (ip6->ip6_nxt == 0x11) // UDP is the next header
            offset += sizeof(click_ip6);
        else // the next header is not UDP so stop now
        {
            p->kill();
            return 0;
        }
    }
    else
    {
        click_chatter("Unknown IP version!");
        p->kill();
        return 0;
    }

    if (p->length() < offset + sizeof(click_udp) + sizeof(PData))
    {
        //click_chatter("Packet is too short");
        p->kill();
        return 0;
    }
    // eth, IP, headers must be bypassed with a correct offset
    udph = (click_udp*)(p->data() + offset);
    pData = (PData*)((char*)udph + sizeof(click_udp));
    csum = udph->uh_sum;

    // we use incremental checksum computation to patch up the checksum after
    // the payload will get modified
    _count++;
    if (_delta)
    {
        Timestamp ts1(ntohl(pData->data[0]), Timestamp::nsec_to_subsec(ntohl(pData->data[1])));
        Timestamp diff = Timestamp::now() - ts1;

        //click_chatter("Seq %d Time Diff sec: %d usec: %d\n", ntohl(pData->seq_num), diff.sec(), diff.nsec());
        pData->data[2] = htonl(diff.sec());
        pData->data[3] = htonl(diff.nsec());
        csum += click_in_cksum((const unsigned char*)(pData->data + 2), 2 * sizeof(uint32_t));
    }
    else
    {
        tnow = Timestamp::now();

        // subtract the previous contribution from CHECKSUM in case PData values are non-zero
        csum -= click_in_cksum((const unsigned char*)pData, sizeof(PData));
        pData->seq_num = htonl(_count);
        pData->data[0] = htonl(tnow.sec());
        pData->data[1] = htonl(tnow.nsec());
        pData->data[2] = 0;
        pData->data[3] = 0;
        csum += click_in_cksum((const unsigned char*)pData, sizeof(PData));
    }

    csum = (0xFFFF & csum) + ((0xFFFF0000 & csum) >> 16);
    csum = (csum != 0xFFFF) ? csum : 0;

    // now that we modified the packet it is time to fix up the
    // csum of the header.
    udph->uh_sum = (uint16_t)csum;

    return p;
}


int StoreUDPTimeSeqRecord::reset_handler(const String &, Element *e, void *, ErrorHandler *)
{
    StoreUDPTimeSeqRecord *t = (StoreUDPTimeSeqRecord *) e;

    t->_count = 0;
    return 0;
}

void StoreUDPTimeSeqRecord::add_handlers()
{
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_write_handler("reset", reset_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StoreUDPTimeSeqRecord)
