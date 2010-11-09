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
#include "udptimecheck.hh"
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <stdio.h>



CLICK_DECLS

/*
 */


UDPTimeCheck::UDPTimeCheck()
{
    _count = 0;
    _in = 1;
    _csum = 1;
    _offset = 0;
}

UDPTimeCheck::~UDPTimeCheck()
{
}



int UDPTimeCheck::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                    "OFFSET", cpkP+cpkM, cpInteger, &_offset,
                    "IN", 0, cpBool, &_in,
                    "CSUM", 0, cpBool, &_csum,
                    cpEnd) < 0)
        return -1;
    return 0;
}

// This is the tricky bit.  We can't rely on any headers being
// filled out if elements like CheckIPHeader have not been called
// so we should just access the raw data and cast wisely
Packet* UDPTimeCheck::simple_action(Packet *packet)
{
    WritablePacket *p = packet->uniqueify();
    Timestamp       tnow;
    click_udp      *udph = 0;
    PData          *pData;
    uint32_t        csum = 0;

    // the packet is shared and we can't modify it without screwing
    // things up
    if (!p)
    {
        // If uniqueify() fails, packet itself is garbage and has been deleted
        click_chatter("Non-Writable Packet!");
        return 0;
    }

    // eth, IP, headers must be bypassed with a correct offset
    // shift_data will deal with alignment issues if necessary
    if (!(p = p->shift_data(_offset)->uniqueify())) // if NULL then the packet is deleted
        return 0;
    udph = (click_udp*)(p->data());

    //udph = (click_udp*)(p->data() + _offset);

    pData = (PData*)((char*)udph + 8);
    csum = udph->uh_sum;

    // we use incremental checksum computation to patch up the checksum after
    // the payload will get modified
    _count++;
    if (_in)
    {
        pData->data[0] = ntohl(pData->data[0]);
        pData->data[1] = ntohl(pData->data[1]);

        Timestamp ts1(pData->data[0], Timestamp::nsec_to_subsec(pData->data[1]));
        Timestamp diff = Timestamp::now() - ts1;

        pData->data[2] = diff.sec();
        pData->data[3] = diff.nsec();
        pData->seq_num = ntohl(pData->seq_num);
        csum += click_in_cksum((const unsigned char*)(pData->data + 2), 2 * sizeof(uint32_t));
        click_chatter("Time Diff sec: %d usec: %d\n", pData->data[2], pData->data[3]);
    }
    else
    {
        tnow = Timestamp::now();

        pData->seq_num = htonl(_count);
        pData->data[0] = htonl(tnow.sec());
        pData->data[1] = htonl(tnow.nsec());
        pData->data[2] = 0;
        pData->data[3] = 0;
        csum += click_in_cksum((const unsigned char*)pData, sizeof(PData));
    }

    if (!(p = p->shift_data(-_offset)->uniqueify())) // if NULL then the packet is deleted
        return 0;

    if (!_csum)
        return p;

    csum = (0xFFFF & csum) + ((0xFFFF0000 & csum) >> 16);
    csum = (csum != 0xFFFF) ? csum : 0;

    // now that we modified the packet it is time to fix up the
    // csum of the header.
    udph->uh_sum = (uint16_t)csum;


    // in HEX you can see the data easily in the ethereal data output
    //click_chatter("SeqNum: %d", pData->seq_num);    
    return p;
}


int UDPTimeCheck::reset_handler(const String &, Element *e, void *, ErrorHandler *)
{
    UDPTimeCheck *t = (UDPTimeCheck *) e;

    t->_count = 0;
    return 0;
}

void UDPTimeCheck::add_handlers()
{
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_write_handler("reset", reset_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(UDPTimeCheck)
