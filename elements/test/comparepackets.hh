// -*- c-basic-offset: 4 -*-
#ifndef CLICK_COMPAREPACKETS_HH
#define CLICK_COMPAREPACKETS_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

ComparePackets([I<keywords> TIMESTAMP])

=s test

compare packets in pairs

=d

ComparePackets compares packets pulled from the first input with packets
pulled from the second input.  Packets are considered different if they have
different length, data, header offsets, or timestamp annotations.

Keyword arguments are:

=over 8

=item TIMESTAMP

Boolean.  If true, then ComparePackets will check packet timestamp
annotations.  Default is true.

=back

=h diffs read-only

Returns the number of different packet pairs seen.

=h diff_details read-only

Returns a text file showing how many different packet pairs ComparePackets has
seen, subdivided by type of difference.

=h all_same read-only

Returns "true" iff all packet pairs seen so far have been identical.

=a

PacketTest */

class ComparePackets : public Element { public:

    ComparePackets() CLICK_COLD;

    const char *class_name() const		{ return "ComparePackets"; }
    const char *port_count() const		{ return "2/2"; }
    const char *processing() const		{ return PULL; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *pull(int);

  private:

    Packet *_p[2];
    bool _available[2];
    NotifierSignal _signal[2];

    bool _timestamp : 1;

    uint32_t _ndiff;
    enum { D_LEN, D_DATA, D_TIMESTAMP, D_NETOFF, D_NETLEN, D_NETHDR,
	   D_MORE_PACKETS_0, D_MORE_PACKETS_1, D_LAST };
    uint32_t _diff_details[D_LAST];

    void check(Packet *, Packet *);
    static String read_handler(Element *, void *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
