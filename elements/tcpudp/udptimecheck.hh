// -*- c-basic-offset: 4 -*-
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
#ifndef CLICK_UDPTIMECHECK_HH
#define CLICK_UDPTIMECHECK_HH
#include <click/element.hh>



/*
UDPTimeCheck element can be handy when computing end-to-end UDP packet
delays.  The element embeds a timestamp and a sequence number into a packet and 
adjusts the checksum of the UDP packet.  Once the initial timestamp has been 
placed into the payload of the UDP packet, a time difference can be computed 
once a packet passes through another UDPTimeCheck element.  The data can be 
accessed by examining the packet payload (e.g., do a tcpdump and then do post 
processing of the data).  The element uses partial checksums to speed up the 
processing.

Packet payload found after the UDP header.  Note, the UDP payload must be at least 
22 bytes long.

uint32_t seq_num
uint32_t initial_second 
uint32_t initial_nano_second 
uint32_t difference_second 
uint32_t difference_nano_second

Keyword arguments are:

=over 3

=item OFFSET

Number of bytes to offset from the beginning of the packet where the IP header can be found.
If raw Ethernet packets are fed into this element, then OFFSET needs to be 14.

=item IN

Determines if the time difference needs to be computed or if the initial timestamp needs to 
be set.  If IN is false, then the initial timestamp is set.  If IN is true, then the 
time difference is computed between the initial timestamp and now.

=itme CSUM

If CSUM is true, then UDP checksum is fixed each time the payload data is changed.

=back

=e
    src :: RatedSource(\<00>, LENGTH 22, RATE 1, LIMIT 100)
        -> UDPIPEncap(10.0.1.1, 6667, 20.0.0.2, 6667)
        -> EtherEncap(0x0800, 00:04:23:D0:93:63, 00:17:cb:0d:f8:db)
        -> UDPTimeCheck(OFFSET 34, IN false, CSUM true)
        -> DelayShaper(100msec)
        -> UDPTimeCheck(OFFSET 34, IN true, CSUM true)
        -> ToDump("dump.dmp");
*/
class UDPTimeCheck : public Element
{
public:
    UDPTimeCheck();
    ~UDPTimeCheck();
  
    const char *class_name() const	{ return "UDPTimeCheck"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return AGNOSTIC; }

    void add_handlers();
    int  configure(Vector<String> &conf, ErrorHandler *errh);

    Packet *simple_action(Packet *);

    // packet data payload access struct
    // Header | PDATA | rest of data
    // This comes out to 22 bytes which will fit into the smallest Ethernet frame
    struct PData
    {
        uint32_t  seq_num;
        uint32_t  data[4];
    };

private:
    unsigned long _count;
    bool          _in;  // if true put in_timestamp else out_timestamp
    bool          _csum; // do we fix the checksums or not
    uint32_t      _offset; //how much to shift to get to the IPv4 header

    static String read_handler(Element *, void *);
    static int    reset_handler(const String &, Element *, void *, ErrorHandler *);
};


#endif
