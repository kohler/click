// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TRUNCATEIPPAYLOAD_HH
#define CLICK_TRUNCATEIPPAYLOAD_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

TruncateIPPayload([LENGTH, TRANSPORT])

=s basicmod

limit payload length

=d

Shorten IP packets to have payload of at most LENGTH bytes.  Normally
"payload" begins at the end of the transport header (TruncateIPPayload
understands TCP, UDP, and ICMP headers).  If TRANSPORT is false, "payload"
begins at the end of the IP header.  Non-IP packets are truncated to at most
LENGTH bytes.  Default LENGTH is 0.

The EXTRA_LENGTH keyword argument determines whether packets' extra length
annotations are updated to account for any dropped bytes.  Default is true.

=a Truncate
*/

class TruncateIPPayload : public Element { public:

    TruncateIPPayload() CLICK_COLD;
    ~TruncateIPPayload() CLICK_COLD;

    const char *class_name() const		{ return "TruncateIPPayload"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    unsigned _nbytes;

};

CLICK_ENDDECLS
#endif
